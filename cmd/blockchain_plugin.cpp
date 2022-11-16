#include "blockchain_plugin.hpp"

#include <iostream>
#include <limits>
#include <map>
#include <string>

#include <silkworm/stagedsync/sync_loop.hpp>
#include <silkworm/stagedsync/stage_direct_bodies.hpp>
#include <silkworm/downloader/internals/header_persistence.hpp>
#include <silkworm/downloader/internals/body_persistence.hpp>

using sys = sys_plugin;
class blockchain_plugin_impl : std::enable_shared_from_this<blockchain_plugin_impl> {
   public:
      blockchain_plugin_impl() = default;

      inline void init() {
         SILK_DEBUG << "blockchain_plugin_impl INIT";
         db_env = appbase::app().get_plugin<engine_plugin>().get_db();
         node_settings = appbase::app().get_plugin<engine_plugin>().get_node_settings();
         SILK_INFO << "Using DB environment at location : " << node_settings->data_directory->chaindata().path().string();

         sync_loop = std::make_unique<silkworm::stagedsync::SyncLoop>(node_settings, db_env, block_queue);
         sync_loop->start(/*wait=*/false);

         evm_blocks_subscription = appbase::app().get_channel<channels::evm_blocks>().subscribe(
            [this](auto b) {
               try {
                   //SILK_INFO << "EVM Block " << b->header.number;
                   block_queue.push(b);

                   if( sync_loop->get_state() == silkworm::Worker::State::kStopped ) {
                       appbase::app().quit();
                   }
               } catch (const mdbx::exception& ex) {
                  SILK_CRIT << "CALLBACK ERR1" << std::string(ex.what());
               } catch (const std::exception& ex) {
                  SILK_CRIT << "CALLBACK ERR2" << std::string(ex.what());
               } catch (...) {
                  SILK_CRIT << "CALLBACK ERR3";
               }
            }
         );
      }

      void shutdown() {
         sync_loop->stop(true);
      }

      using txn_t = std::unique_ptr<silkworm::db::RWTxn>;

      silkworm::NodeSettings*                            node_settings;
      mdbx::env*                                         db_env;
      channels::evm_blocks::channel_type::handle         evm_blocks_subscription;
      std::unique_ptr<silkworm::stagedsync::SyncLoop>    sync_loop;
      silkworm::stagedsync::DirectBodiesStage::BlockQueue block_queue;
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