#include "block_conversion_plugin.hpp"
#include "channels.hpp"
#include "abi_utils.hpp"

#include <fstream>

#include <silkworm/types/transaction.hpp>

class block_conversion_plugin_impl : std::enable_shared_from_this<block_conversion_plugin_impl> {
   public:
      block_conversion_plugin_impl()
        : block_event(appbase::app().get_channel<channels::blocks>()),
          fork_event(appbase::app().get_channel<channels::forks>()),
          lib_event(appbase::app().get_channel<channels::lib_advance>()) {}

      inline void init(uint32_t stride, std::string abi_dir) {
         block_stride = stride;

         load_abi(abi_dir);

         native_blocks_subscription = appbase::app().get_channel<channels::native_blocks>().subscribe(
            [this](auto b) {
               if (b->block_num > current_height) {
                  SILK_DEBUG << "Advancing current height to " << b->block_num;
                  current_height = b->block_num;
               } else if (b->block_num < current_height) {
                  SILK_WARN << "Fork encountered at block " << b->block_num << " current head is at " << current_height;
               }

               if (b->block_num > next_block_bound) {
                  SILK_DEBUG << "Creating EVM Block " << b->block_num;
                  if (b->syncing)
                     SILK_DEBUG << "Block is Syncing";
                  current_block = {};
                  current_block.header.number     = b->block_num;
                  current_block.header.difficulty = 0;
                  current_block.header.gas_limit  = std::numeric_limits<uint64_t>::max();
                  current_block.header.timestamp  = b->timestamp / 1000;
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
                     current_block.transactions.emplace_back(std::move(trx));
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
      channels::native_blocks::channel_type::handle native_blocks_subscription;
      uint32_t                                      block_stride = 0;
      uint32_t                                      current_height = 0;
      uint32_t                                      next_block_bound = 0;
      uint32_t                                      current_lib = 0;
      channels::block                               current_block;
      abieos::abi                                   abi;
};

block_conversion_plugin::block_conversion_plugin() : my(new block_conversion_plugin_impl()) {}
block_conversion_plugin::~block_conversion_plugin() {}

void block_conversion_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("block-stride", boost::program_options::value<uint32_t>()->default_value(10),
        "block stride from native blocks to Trust EVM blocks")
      ("evm-abi", boost::program_options::value<std::string>()->default_value("./evm.abi"),
        "ABI for TrustEVM runtime")

   ;
}

void block_conversion_plugin::plugin_initialize( const appbase::variables_map& options ) {
   auto stride  = options.at("block-stride").as<uint32_t>();
   auto abi_dir = options.at("evm-abi").as<std::string>();
   my->init(stride, abi_dir);
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