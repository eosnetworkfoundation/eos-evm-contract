#include "engine_plugin.hpp"
#include "channels.hpp"

#include <filesystem>
#include <iostream>
#include <string>

#include <boost/process/environment.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>

#include <nlohmann/json.hpp>

#include <silkworm/common/stopwatch.hpp>
#include <silkworm/concurrency/signal_handler.hpp>
#include <silkworm/db/stages.hpp>
#include <silkworm/db/access_layer.hpp>
#include <silkworm/db/genesis.hpp>
#include <silkworm/rpc/server/backend_kv_server.hpp>
#include <silkworm/rpc/util.hpp>

using sys = sys_plugin;
class engine_plugin_impl : std::enable_shared_from_this<engine_plugin_impl> {
   public:
      engine_plugin_impl(const std::string& data_dir, uint32_t num_of_threads, uint32_t max_readers, std::string address, std::optional<std::string> genesis_json) {

         node_settings.data_directory = std::make_unique<silkworm::DataDirectory>(data_dir, false);
         node_settings.etherbase  = silkworm::to_evmc_address(silkworm::from_hex("").value()); // TODO determine etherbase name
         node_settings.chaindata_env_config = {node_settings.data_directory->chaindata().path().string(), false, false};
         node_settings.chaindata_env_config.max_readers = max_readers;
         node_settings.chaindata_env_config.exclusive = false;
         node_settings.prune_mode = std::make_unique<silkworm::db::PruneMode>();

         server_settings.set_address_uri(address);
         server_settings.set_num_contexts(num_of_threads);

         pid = boost::this_process::get_id();
         tid = std::this_thread::get_id();

         const auto data_path = std::filesystem::path(node_settings.data_directory->chaindata().path().string());
         if ( std::filesystem::exists(data_path) ) {
            node_settings.chaindata_env_config.shared = true;
         } else {
            node_settings.chaindata_env_config.create = true;
         }

         db_env = silkworm::db::open_env(node_settings.chaindata_env_config);
         SILK_INFO << "Created DB environment at location : " << node_settings.data_directory->chaindata().path().string();

         silkworm::db::RWTxn txn(db_env);
         silkworm::db::table::check_or_create_chaindata_tables(txn);

         auto existing_config{silkworm::db::read_chain_config(txn)};
         if (!existing_config.has_value()) {
            if(!genesis_json) {
               sys::error("Genesis state not provided");
               return;
            }

            boost::filesystem::path genesis_file = *genesis_json;
            if(!boost::filesystem::is_regular_file(genesis_file)) {
               sys::error("Specified genesis file does not exist");
               return;
            }

            std::ifstream genesis_f(genesis_file);
            silkworm::db::initialize_genesis(txn, nlohmann::json::parse(genesis_f), /*allow_exceptions=*/true);
         }

         txn.commit();
         node_settings.chain_config = silkworm::db::read_chain_config(txn);
         node_settings.network_id = node_settings.chain_config->chain_id;

         eth.reset(new silkworm::EthereumBackEnd(node_settings, &db_env));
         eth->set_node_name("EOS EVM Node");
         SILK_INFO << "Created Ethereum Backend with network id <" << node_settings.network_id << ">, etherbase <" << node_settings.etherbase->bytes << ">";

         server.reset(new silkworm::rpc::BackEndKvServer(server_settings, *eth.get()));
      }

      inline void startup() {
         server->build_and_start();
         SILK_INFO << "Started Engine Server";
      }

      inline void shutdown() {
         eth->close();
         server->shutdown();
         SILK_INFO << "Stopped Engine Server";
      }

      std::optional<silkworm::BlockHeader> get_head_canonical_header() {
         silkworm::db::ROTxn txn(db_env);
         auto head_num = silkworm::db::stages::read_stage_progress(txn, silkworm::db::stages::kHeadersKey);
         return silkworm::db::read_canonical_header(txn, head_num);
      }

      std::optional<silkworm::Block> get_head_block() {
         auto header = get_head_canonical_header();
         if(!header) return {};

         silkworm::db::ROTxn txn(db_env);
         silkworm::Block block;
         auto res = read_block_by_number(txn, header->number, false, block);
         if(!res) return {};
         return block;
      }

      std::optional<silkworm::BlockHeader> get_genesis_header() {
         silkworm::db::ROTxn txn(db_env);
         return silkworm::db::read_canonical_header(txn, 0);
      }

      silkworm::NodeSettings                          node_settings;
      silkworm::rpc::ServerConfig                     server_settings;
      mdbx::env_managed                               db_env;
      std::unique_ptr<silkworm::EthereumBackEnd>      eth;
      std::unique_ptr<silkworm::rpc::BackEndKvServer> server;
      int                                             pid;
      std::thread::id                                 tid;
      std::optional<std::string>                      genesis_json;
};

engine_plugin::engine_plugin() {}
engine_plugin::~engine_plugin() {}

void engine_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("chain-data", boost::program_options::value<std::string>()->default_value("."),
        "chain data path as a string")
      ("engine-port", boost::program_options::value<std::string>()->default_value("127.0.0.1:8080"),
        "engine address of the form <address>:<port>")
      ("threads", boost::program_options::value<std::uint32_t>()->default_value(16),
        "number of worker threads")
      ("genesis-json", boost::program_options::value<std::string>(),
        "file to read EVM genesis state from")
      ("max-readers", boost::program_options::value<std::uint32_t>()->default_value(1024),
        "maximum number of database readers")
   ;
}

void engine_plugin::plugin_initialize( const appbase::variables_map& options ) {
   const auto& chain_data = options.at("chain-data").as<std::string>();
   const auto& address    = options.at("engine-port").as<std::string>();
   const auto& threads    = options.at("threads").as<uint32_t>();
   const auto& max_readers = options.at("max-readers").as<uint32_t>();

   std::optional<std::string> genesis_json;
   if(options.count("genesis-json"))
      genesis_json = options.at("genesis-json").as<std::string>();

   my.reset(new engine_plugin_impl(chain_data, threads, max_readers, address, genesis_json));
   SILK_INFO << "Initializing Engine Plugin";
}

void engine_plugin::plugin_startup() {
   my->startup();
}

void engine_plugin::plugin_shutdown() {
   my->shutdown();
   SILK_INFO << "Shutdown engine plugin";
}

mdbx::env* engine_plugin::get_db() {
   return &my->db_env;
}

std::optional<silkworm::BlockHeader> engine_plugin::get_head_canonical_header() {
   return my->get_head_canonical_header();
}

std::optional<silkworm::Block> engine_plugin::get_head_block() {
   return my->get_head_block();
}

std::optional<silkworm::BlockHeader> engine_plugin::get_genesis_header() {
   return my->get_genesis_header();
}

silkworm::NodeSettings* engine_plugin::get_node_settings() {
   return &my->node_settings;
}

std::string engine_plugin::get_address() const {
   return my->server_settings.address_uri();
}

uint32_t engine_plugin::get_threads() const {
   return my->server_settings.num_contexts();
}

std::string engine_plugin::get_chain_data_dir() const {
   return my->node_settings.data_directory->chaindata().path().string();
}