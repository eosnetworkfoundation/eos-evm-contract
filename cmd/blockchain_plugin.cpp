#include "blockchain_plugin.hpp"

#include <iostream>
#include <limits>
#include <map>
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
         block_stride = appbase::app().get_plugin<block_conversion_plugin>().get_block_stride();
         db_env = appbase::app().get_plugin<engine_plugin>().get_db();
         node_settings = appbase::app().get_plugin<engine_plugin>().get_node_settings();
         engine.reset(new silkworm::consensus::NoProofEngine(*(node_settings->chain_config)));
         SILK_INFO << "Using DB environment at location : " << node_settings->data_directory->chaindata().path().string();

         blocks_subscription = appbase::app().get_channel<channels::blocks>().subscribe(
            [this](auto b) {
               this->execute_block(*b, 0, 0);
            }
         );
         forks_subscription = appbase::app().get_channel<channels::forks>().subscribe(
            [this](auto f) {
               this->unwind(f->block_num);
            }
         );
         lib_subscription = appbase::app().get_channel<channels::lib_advance>().subscribe(
            [this](auto l) {
               this->commit(l->block_num);
            }
         );
      }


      void unwind(uint32_t block_num) {
         const auto it = txns.lower_bound(block_num);
         if (it == txns.end()) {
            SILK_CRIT << "block not found, internal failure";
            throw std::runtime_error("internal error");
         }
         txns.erase(it, txns.end()); // remove all transactions after this point RAII should invalidate the mdbx managed transactions
      }

      void commit(uint32_t block_num) {
         const auto it = txns.upper_bound(block_num);
         if (it == txns.end()) {
            SILK_CRIT << "block not found, internal failure";
            throw std::runtime_error("internal error");
         }
         for (auto i = txns.begin(); i != it; ++i) {
            i->second->commit();
         }
      }

      void execute_block(silkworm::Block& block, std::size_t core_block_height, std::size_t core_lib) {
         if (core_block_height > next_core_height) {
            auto txn = std::make_unique<silkworm::db::RWTxn>(*db_env);
            current_txn = txn.get();
            txns.emplace(core_block_height, std::move(txn));
            next_core_height = core_block_height + block_stride;
         }
         const auto t = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>(std::chrono::microseconds(block.header.timestamp)));

         SILK_INFO << "Executing Block : " << block.header.number << " at time " << std::ctime(&t);
         try {
            silkworm::db::Buffer buff{*(*current_txn), 0};
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
      std::map<uint32_t, txn_t>                          txns;
      silkworm::db::RWTxn*                               current_txn;
      std::size_t                                        next_core_height;
      std::size_t                                        core_head_block;
      uint32_t                                           block_stride;
      channels::blocks::channel_type::handle             blocks_subscription;
      channels::forks::channel_type::handle              forks_subscription;
      channels::lib_advance::channel_type::handle        lib_subscription;
      silkworm::BaselineAnalysisCache                    analysis_cache{std::size_t(5000)};
      silkworm::ObjectPool<silkworm::EvmoneExecutionState> state_pool;
};

blockchain_plugin::blockchain_plugin() : my(new blockchain_plugin_impl()) {}
blockchain_plugin::~blockchain_plugin() {}

void blockchain_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
}

void blockchain_plugin::plugin_initialize( const appbase::variables_map& options ) {
   my->init();
   SILK_INFO << "Initialized Blockchain Plugin";
}

void blockchain_plugin::plugin_startup() {
   SILK_INFO << "Starting Blockchain Plugin";
}

void blockchain_plugin::plugin_shutdown() {
   SILK_INFO << "Shutdown Blockchain plugin";
}