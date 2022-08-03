#include "block_conversion_plugin.hpp"
#include "channels.hpp"

#include <fstream>

#include <eosio/abi.hpp>
#include <abieos.hpp>

#include <silkworm/types/transaction.hpp>

class block_conversion_plugin_impl : std::enable_shared_from_this<block_conversion_plugin_impl> {
   public:
      block_conversion_plugin_impl()
        : block_event(appbase::app().get_channel<channels::blocks>()){}

      inline void init(uint32_t stride, std::string abi_dir) {
         block_stride = stride;

         std::ifstream abi_file(abi_dir);
         if (abi_file.is_open()) {
            SILK_CRIT << "ABI file <" << abi_dir << "> not found";
            throw std::runtime_error("Failed to find ABI file");
         }
         std::size_t len = 0;
         abi_file.seekg(0, std::ios::end);
         len = abi_file.tellg();
         abi_file.seekg(0, std::ios::beg);

         char* buffer = new char[len];
         abi_file.read(buffer, len);
         abi_file.close();

         eosio::json_token_stream stream = {buffer};
         eosio::abi_def def = eosio::from_json<eosio::abi_def>(stream);
         SILK_DEBUG << "TrustEVM ABI Version " << def.version;
         eosio::convert(def, abi);

         delete[] buffer;

         native_blocks_subscription = appbase::app().get_channel<channels::native_blocks>().subscribe(
            [this](auto b) {
               if (b->block_num > next_block_bound) {
                  SILK_INFO << "Creating EVM Block " << b->block_num;
                  current_block = {};
                  current_block.header.number     = b->block_num;
                  current_block.header.difficulty = 0;
                  current_block.header.gas_limit  = std::numeric_limits<uint64_t>::max();
                  current_block.header.timestamp  = b->timestamp / 1000;
               }
               SILK_INFO << b->transactions.size() << " # of transactions";
               for (const auto& ntrx : b->transactions) {
                  for (const auto& act : ntrx.actions) {
                     silkworm::Transaction trx;
                     silkworm::ByteView    view = {(const uint8_t*)act.data.pos, act.data.remaining()};
                     SILK_INFO << "Decoding Transaction";
                     if (silkworm::rlp::decode<silkworm::Transaction>(view, trx) != silkworm::DecodingResult::kOk) {
                        SILK_CRIT << "Failed to decode transaction";
                        throw std::runtime_error("Failed to decode transaction");
                     }
                     current_block.transactions.emplace_back(std::move(trx));
                  }
               }
            }
         );
      }

      const auto& get_type(const std::string& n) {
         auto it = abi.abi_types.find(n);
         if (it == abi.abi_types.end())
            throw std::runtime_error("unknown ABI type <"+n+">");
         return it->second;
      }

      inline void shutdown() {
      }

      channels::blocks::channel_type&               block_event;
      channels::native_blocks::channel_type::handle native_blocks_subscription;
      uint32_t                                      block_stride;
      uint32_t                                      current_height;
      uint32_t                                      next_block_bound;
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