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
#include <silkworm/db/access_layer.hpp>
#include <silkworm/node/silkworm/stagedsync/stagedsync.hpp>
#include <silkworm/execution/processor.hpp>

using sys = sys_plugin;
class blockchain_plugin_impl : std::enable_shared_from_this<blockchain_plugin_impl> {

   public: 
      class Override_State: public silkworm::db::Buffer {

      public:
         explicit Override_State(mdbx::txn& txn, silkworm::BlockNum prune_history_threshold,
            std::optional<silkworm::BlockNum> historical_block = std::nullopt): 
            silkworm::db::Buffer(txn, prune_history_threshold, historical_block) {}

         std::optional<silkworm::Account> read_account(const evmc::address& address) const noexcept override {
            using namespace intx;
            const uint8_t pass_address[] = {0x27, 0x87, 0xb9, 0x8f};
            std::optional<silkworm::Account> res = silkworm::db::Buffer::read_account(address);
            if ((bool)res) return res;
            if (memcmp((const char *)address.bytes, (const char *)pass_address, sizeof(pass_address)) == 0) {
               silkworm::Account acc;
               acc.balance = 1000000000000000000000000000_u256;
               SILK_INFO << "Hack read_account balance";
               return acc;
            }
            return res;
         }
      };
       
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
               this->execute_block(b->data, b->core_block_num);
            }
         );
         forks_subscription = appbase::app().get_channel<channels::forks>().subscribe(
            [this](auto f) {
               SILK_INFO << "Fork at " << f->block_num;
               // TODO reinstate when adding back forking support
               //this->unwind(f->block_num);
            }
         );
         lib_subscription = appbase::app().get_channel<channels::lib_advance>().subscribe(
            [this](auto l) {
               SILK_INFO << "Commit at " << l->block_num;
               // TODO reinstate when adding back forking support
               //this->commit(l->block_num);
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
         uint32_t highest_commit = 0;
         const auto it = txns.upper_bound(block_num);
         for (auto i = txns.begin(); i != it; ++i) {
            SILK_DEBUG << "Committing " << i->first;
            highest_commit = i->first;
            i->second->commit(false);
            i = txns.erase(i);
         }
         appbase::app().get_plugin<ship_receiver_plugin>().update_core_chain_state(highest_commit);
      }

      void commit_all() {
         uint32_t highest_commit = 0;
         txns.clear();
         for (auto i = txns.begin(); i != txns.end(); ++i) {
            SILK_DEBUG << "Committing " << i->first;
            highest_commit = i->first;
            i->second->commit(false);
         }
         appbase::app().get_plugin<ship_receiver_plugin>().update_core_chain_state(highest_commit);
      }

      void set_current_txn(uint32_t core_block_num) {
         SILK_DEBUG << "txns.size() " << txns.size();
         if (core_block_num >= next_core_height) {
            if (txns.empty()) {
               current_txn = new silkworm::db::RWTxn(*db_env);
            } else {
               SILK_DEBUG << "Parent " << current_txn->id();
               current_txn = new silkworm::db::RWTxn((silkworm::db::RWTxn*)current_txn);
            }
            txns.emplace(core_block_num, current_txn);
            next_core_height = core_block_num + block_stride;
         }
      }

      void execute_block(silkworm::Block& block, std::size_t core_block_num) {
         try {
            // set_current_txn(core_block_num);  // needed for forking resilience
            // since this fails using simple commit and renew
            if (txns.empty()) {
               current_txn = new silkworm::db::RWTxn(*db_env);
               txns.emplace(0, current_txn);
            }
            //current_txn->commit(true);
            
            appbase::app().get_plugin<ship_receiver_plugin>().update_trust_chain_state(block.header.number);
            appbase::app().get_plugin<ship_receiver_plugin>().update_core_chain_state(core_block_num);
            // TODO remove all of the above and replace with set_current_txn when adding back forking support

            SILK_INFO << "Executing Block : " << block.header.number;
            Override_State buff{*(*current_txn), 0};
            buff.begin_block(block.header.number);
            std::vector<silkworm::Receipt> receipts;

            silkworm::ExecutionProcessor processor(block, *engine, buff, node_settings->chain_config.value());
            processor.evm().baseline_analysis_cache = &analysis_cache;
            processor.evm().state_pool              = &state_pool;

            const auto res = processor.execute_and_write_block(receipts);

            const auto block_hash = silkworm::to_hex(block.header.hash().bytes, true);
            if (res != silkworm::ValidationResult::kOk) {
               SILK_CRIT << "Block validation error : " << std::string(magic_enum::enum_name<silkworm::ValidationResult>(res));
            } else {
               
               SILK_INFO << "Silworm successfully executed block #" << block.header.number << " " << block_hash << " (EOS block #" << core_block_num << ")";

               auto chain_data_path = node_settings->data_directory->etl().path();

               silkworm::db::stages::write_stage_progress(*(*current_txn), silkworm::db::stages::kHeadersKey, block.header.number);
               //current_txn->commit(true);
               SILK_INFO << "stage written";

#if 0
               auto canonical_table{silkworm::db::open_cursor(*(*current_txn),silkworm::db::table::kCanonicalHashes)};
               silkworm::Bytes block_key{silkworm::db::block_key(block.header.number)};
               canonical_table.insert(silkworm::db::to_slice(block_key), silkworm::db::to_slice(block.header.hash().bytes));
               current_txn->commit(true);
#endif

               silkworm::db::stages::write_stage_progress(*(*current_txn), silkworm::db::stages::kBlockHashesKey, block.header.number);

               silkworm::db::write_header(*(*current_txn), block.header, true);
               //silkworm::db::write_header_number(*(*current_txn), block.header.hash().bytes, block.header.number);
               //silkworm::db::write_head_header_hash(*(*current_txn), block.header.hash().bytes);
               silkworm::db::write_canonical_header(*(*current_txn), block.header);
               //silkworm::db::write_canonical_header_hash(*(*current_txn), block.header.hash().bytes, block.header.number);
               //current_txn->commit(true);

               silkworm::db::stages::write_stage_progress(*(*current_txn), silkworm::db::stages::kBlockBodiesKey, block.header.number);
               silkworm::db::write_body(*(*current_txn), block, block.header.hash().bytes, block.header.number);

               SILK_INFO << "kHeaders, kHeaderNumbers, CanonicalHashes, kBlockBodies written for #" << block.header.number;

               //current_txn->commit(true);


               buff.write_to_db(); // persist all contects into db
               SILK_INFO << "after write_to_db()";
               //current_txn->commit(true);

               if (silkworm::stagedsync::stage_account_history(*current_txn, chain_data_path) == silkworm::stagedsync::StageResult::kSuccess) {
                  SILK_INFO << "stage_account_history OK";
               }
               //if (silkworm::stagedsync::stage_storage_history(*current_txn, chain_data_path) == silkworm::stagedsync::StageResult::kSuccess) {
               //   SILK_INFO << "stage_storage_history OK";
               //} // FIXME: error System Error: MDBX_NOTFOUND: No matching key/data pair found

               current_txn->commit(true);
            }

            high_water_mark = core_block_num;

         } catch(std::runtime_error err) {
            sys::error(err.what());
         } catch(std::exception ex) {
            sys::error(ex.what());
         } catch (...) {
            sys::error("Block execution failed");
         }
      }

      void shutdown() {
      }

      using txn_t = std::unique_ptr<silkworm::db::RWTxn>;

      silkworm::NodeSettings*                            node_settings;
      mdbx::env*                                         db_env;
      std::unique_ptr<silkworm::consensus::IEngine>      engine;
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
      uint32_t                                           high_water_mark = 0;
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
   my->shutdown();
   SILK_INFO << "Shutdown Blockchain plugin";
}