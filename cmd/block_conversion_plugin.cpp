#include "block_conversion_plugin.hpp"
#include "channels.hpp"
#include "abi_utils.hpp"
#include "chain_state.hpp"

#include <fstream>

#include <silkworm/types/transaction.hpp>

class block_conversion_plugin_impl : std::enable_shared_from_this<block_conversion_plugin_impl> {
   public:
      block_conversion_plugin_impl()
        : block_event(appbase::app().get_channel<channels::blocks>()),
          fork_event(appbase::app().get_channel<channels::forks>()),
          lib_event(appbase::app().get_channel<channels::lib_advance>()) {}

      inline int64_t get_block_num(int64_t ts) const {
         const int64_t genesis_time = current_chain_state->genesis_time; 
         return genesis_time == 0 ? -1 : ts - genesis_time;
      }

      inline int64_t get_timestamp(int64_t b) const {
         const int64_t genesis_time = current_chain_state->genesis_time;
         return genesis_time == 0 ? -1 : genesis_time - b;
      }

      inline void generate_empty_blocks(uint32_t n) {
         for (std::size_t i=0; i < n; ++i) {
            auto b = std::make_shared<channels::block>();
            const int64_t block_number = current_evm_height+1+i;
            
            b->data.header.number = block_number;
            b->data.header.difficulty = 0;
            b->data.header.gas_limit = std::numeric_limits<uint64_t>::max();
            b->data.header.timestamp =  get_timestamp(block_number)
         }
      }

      inline void init(uint32_t stride, std::string abi_dir, uint32_t block_num) {
         load_abi(abi_dir);
         
         force_emit_subscription = appbase::app().get_channel<channels::force_emit>().subscribe(
            [this](auto r) {
               int64_t block_number = get_block_num(r->timestamp/1000);
               if (block_number > current_evm_height) {
                  if (current_block) {
                     // ship the last constructed block
                     SILK_INFO << "Emitting EVM Block " << current_block->data.header.number;
                     block_event.publish(80, current_block);
                     current_block = nullptr;
                  }
                  generate_empty_blocks(block_number - current_evm_height);
               }
            }
         );

         native_blocks_subscription = appbase::app().get_channel<channels::native_blocks>().subscribe(
            [this](auto b) {
               int64_t block_number = get_block_num(b->timestamp/1000);
               if (b->block_num > current_height) {
                  SILK_DEBUG << "Advancing current height to " << b->block_num;
                  current_height = b->block_num;
                  current_evm_height = block_number;
               } else if (b->block_num < current_height) {
                  SILK_WARN << "Fork encountered at block " << b->block_num << " current head is at " << current_height;
                  fork_event.publish(90, std::make_shared<channels::fork>(b->block_num));
               }

               if (block_number > current_evm_height) {
                  if (current_block) {
                     // ship the last constructed block
                     SILK_INFO << "Emitting EVM Block " << current_block->data.header.number;
                     block_event.publish(80, current_block);
                     current_block = nullptr;
                  }
                  current_block = std::make_shared<channels::block>();

                  SILK_INFO << "Creating EVM Block " << block_number;
                  int64_t block_number = get_block_num(b->timestamp/1000);

                  current_block->data.header.number     = block_number;
                  current_block->data.header.difficulty = 0;
                  current_block->data.header.gas_limit  = std::numeric_limits<uint64_t>::max();
                  current_block->data.header.timestamp  = b->timestamp / 1000;
               }

               if (b->lib > current_lib) {
                  SILK_DEBUG << "Advancing LIB from " << current_lib << " to " << b->lib;
                  current_lib = b->lib;
                  lib_event.publish(80, std::make_shared<channels::lib_change>(current_lib));
               }
               for (const auto& ntrx : b->transactions) {
                  for (const auto& act : ntrx.actions) {
                     silkworm::Transaction trx;
                     auto dtx = deserialize_tx(eosio::input_stream(act.data.pos, act.data.remaining()));
                     silkworm::ByteView bv = {(const uint8_t*)dtx.rlpx.data(), dtx.rlpx.size()};
                     SILK_DEBUG << "Decoding Transaction";
                     if (silkworm::rlp::decode<silkworm::Transaction>(bv, trx) != silkworm::DecodingResult::kOk) {
                        SILK_CRIT << "Failed to decode transaction";
                        throw std::runtime_error("Failed to decode transaction");
                     }
                     trx.from.reset();
                     trx.recover_sender();
                     current_block->data.transactions.emplace_back(std::move(trx));
                  }
               }
            }
         );
      }

      template <typename Stream>
      inline pushtx deserialize_tx(Stream&& s) const {
         pushtx tx;
         eosio::from_bin(tx, s);
         return tx;
      }

      inline uint32_t get_stride() const { return block_stride; }

      void shutdown() {}

      channels::blocks::channel_type&               block_event;
      channels::forks::channel_type&                fork_event;
      channels::lib_advance::channel_type&          lib_event;
      channels::force_emit::channel_type::handle    force_emit_subscription;
      channels::native_blocks::channel_type::handle native_blocks_subscription;
      uint32_t                                      block_stride = 0;
      uint32_t                                      current_height = 0;
      uint32_t                                      current_evm_height = 0;
      uint32_t                                      next_block_bound = 0;
      uint32_t                                      current_lib = 0;
      std::shared_ptr<channels::block>              current_block = nullptr;
      uint32_t                                      next_block_num = 0;
      abieos::abi                                   abi;
      chain_state*                                  current_chain_state = nullptr;
};

block_conversion_plugin::block_conversion_plugin() : my(new block_conversion_plugin_impl()) {}
block_conversion_plugin::~block_conversion_plugin() {}

void block_conversion_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("block-stride", boost::program_options::value<uint32_t>()->default_value(10),
        "block stride from native blocks to Trust EVM blocks")
      ("evm-abi", boost::program_options::value<std::string>()->default_value("./evm_runtime.abi"),
        "ABI for TrustEVM runtime")

   ;
}

void block_conversion_plugin::plugin_initialize( const appbase::variables_map& options ) {
   auto stride  = options.at("block-stride").as<uint32_t>();
   auto abi_dir = options.at("evm-abi").as<std::string>();

   my->current_chain_state = appbase::app().get_plugin<ship_receiver_plugin>().get_chain_state();
   my->init(stride, abi_dir, my->current_chain_state->head_trust_block_num);
   SILK_INFO << "Initialized block_conversion Plugin";
}

void block_conversion_plugin::plugin_startup() {
}

void block_conversion_plugin::plugin_shutdown() {
   my->shutdown();
   SILK_INFO << "Shutdown block_conversion plugin";
}

uint32_t block_conversion_plugin::get_block_stride() const {
   return my->get_stride();
}