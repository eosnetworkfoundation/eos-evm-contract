#include "blockchain_plugin.hpp"

#include <iostream>
#include <limits>
#include <string>


#include <boost/circular_buffer.hpp>

#include <silkworm/consensus/engine.hpp>
#include <silkworm/execution/analysis_cache.hpp>
#include <silkworm/execution/evm.hpp>
#include <silkworm/stagedsync/common.hpp>
#include <silkworm/consensus/noproof/engine.hpp>
#include <silkworm/db/buffer.hpp>
#include <silkworm/execution/processor.hpp>

class blockchain_plugin_impl : std::enable_shared_from_this<blockchain_plugin_impl> {
   public:
      blockchain_plugin_impl() = default;

      inline void init() {
         db_env = appbase::app().get_plugin<silk_engine_plugin>().get_db();
         node_settings = appbase::app().get_plugin<silk_engine_plugin>().get_node_settings();
         engine.reset(new silkworm::consensus::NoProofEngine(*(node_settings->chain_config)));
         SILK_INFO << "Using DB environment at location : " << node_settings->data_directory->chaindata().path().string();

         blocks_subscription = appbase::app().get_channel<channels::blocks>().subscribe(
            [this](auto b) {
               this->execute_block(*b, 0, 0);
            }
         );
         txns.emplace_back(std::make_unique<silkworm::db::RWTxn>(*db_env));
      }

      inline void startup() {
      }

      inline void shutdown() {
      }

      void execute_block(silkworm::Block& block, std::size_t core_block_height, std::size_t core_lib) {
         if (core_block_height > next_core_height) {
            txns.emplace_back(std::make_unique<silkworm::db::RWTxn>(*db_env));
            next_core_height = core_block_height + stride;
         }
         const auto t = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>(std::chrono::microseconds(block.header.timestamp)));

         SILK_INFO << "Executing Block : " << block.header.number << " at time " << std::ctime(&t);
         try {
            silkworm::db::Buffer buff{*(*txns.back()), 0};
            std::vector<silkworm::Receipt> receipts;

            silkworm::ExecutionProcessor processor(block, *engine, buff, node_settings->chain_config.value());
            processor.evm().baseline_analysis_cache = &analysis_cache;
            processor.evm().state_pool              = &state_pool;

            const auto res = processor.execute_and_write_block(receipts);

            if (res != silkworm::ValidationResult::kOk) {
               const auto block_hash = silkworm::to_hex(block.header.hash().bytes, true);
               SILK_CRIT << "Block validation error : " << std::string(magic_enum::enum_name<silkworm::ValidationResult>(res));
            }
         } catch (...) {

         }
         //silkworm::ExecutionProcessor processor = {block, *engine}
      }

      using txn_t = std::unique_ptr<silkworm::db::RWTxn>;
      static inline constexpr std::size_t max_prefetched_blocks = 10240;

      silkworm::NodeSettings*                            node_settings;
      mdbx::env*                                         db_env;
      std::unique_ptr<silkworm::consensus::IEngine>      engine;
      silkworm::BlockNum                                 head_block = 0;
      boost::circular_buffer<silkworm::Block>            prefetched_blocks{max_prefetched_blocks};
      std::mutex                                         progress_mtx;
      std::size_t                                        processed_blocks = 0;
      std::size_t                                        processed_transactions = 0;
      std::size_t                                        processed_gas = 0;
      std::vector<txn_t>                                 txns;
      std::size_t                                        stride = 10;
      std::size_t                                        next_core_height;
      std::size_t                                        core_head_block;
      channels::blocks::channel_type::handle             blocks_subscription;
      silkworm::BaselineAnalysisCache                    analysis_cache{std::size_t(5000)};
      silkworm::ObjectPool<silkworm::EvmoneExecutionState> state_pool;
};

blockchain_plugin::blockchain_plugin() : my(new blockchain_plugin_impl()) {}
blockchain_plugin::~blockchain_plugin() {}

void blockchain_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("chain-data", boost::program_options::value<std::string>()->default_value("."),
        "chain data path as a string")
   ;
}

void blockchain_plugin::plugin_initialize( const appbase::variables_map& options ) {
   my->init();
   SILK_INFO << "Initialized Blockchain Plugin";
}

void blockchain_plugin::plugin_startup() {
   my->startup();
}

void blockchain_plugin::plugin_shutdown() {
   my->shutdown();
   SILK_INFO << "Shutdown blockchain plugin";
}