#include "rpc_plugin.hpp"

#include <silkrpc/config.hpp>

#include <iostream>
#include <string>

#include <agrpc/grpc_context.hpp>
#include <grpcpp/grpcpp.h>

#include <silkrpc/common/log.hpp>
#include <silkrpc/daemon.hpp>
#include <silkworm/common/settings.hpp>

class rpc_plugin_impl : std::enable_shared_from_this<rpc_plugin_impl> {
   public:
      rpc_plugin_impl(silkrpc::DaemonSettings settings)
         : settings(settings) {}

      void init(const silkrpc::DaemonSettings& s) {
         settings = s;
      }

      silkrpc::DaemonSettings settings;
};

rpc_plugin::rpc_plugin() {}
rpc_plugin::~rpc_plugin() {}

void rpc_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("http-port", boost::program_options::value<std::string>()->default_value("127.0.0.1:8881"),
        "http port for JSON RPC of the form <address>:<port>")
      ("trust-evm-node", boost::program_options::value<std::string>()->default_value("127.0.0.1:8001"),
        "address to trust-evm-node of the form <address>:<port>")
      ("rpc-threads", boost::program_options::value<uint32_t>()->default_value(16),
        "number of threads for use with rpc")
      ("chaindata", boost::program_options::value<std::string>()->default_value("./"),
        "directory of chaindata")
      ("rpc-max-readers", boost::program_options::value<uint32_t>()->default_value(16),
        "maximum number of rpc readers")
   ;
}

void rpc_plugin::plugin_initialize( const appbase::variables_map& options ) {
   const auto& http_port   = options.at("rpc-endpoint").as<std::string>();
   const auto  threads     = options.at("rpc-threads").as<uint32_t>();
   const auto  max_readers = options.at("rpc-max-readers").as<uint32_t>();

   // TODO when we resolve issues with silkrpc compiling in trust-evm-node then remove 
   // the `trust-evm-node` options and use silk_engine for the address and configuration
   const auto& node_port  = options.at("trust-evm-node").as<std::string>();
   //const auto node_settings   = engine.get_node_settings();
   const auto& data_dir   = options.at("chaindata").as<std::string>();

   auto verbosity         = appbase::app().get_plugin<sys_plugin>().get_verbosity();

   using evmc::operator""_bytes32;
   silkworm::ChainConfig config{
      0,  // chain_id
      0_bytes32,
      silkworm::SealEngineType::kNoProof,
      {
         0,          // Homestead
         0,          // Tangerine Whistle
         0,          // Spurious Dragon
         0,          // Byzantium
         0,          // Constantinople
         0,          // Petersburg
         0,          // Istanbul
      },
   };

   silkworm::NodeSettings node_settings;
   node_settings.data_directory = std::make_unique<silkworm::DataDirectory>(data_dir, false);
   node_settings.network_id = config.chain_id;
   node_settings.etherbase  = silkworm::to_evmc_address(silkworm::from_hex("").value()); // TODO determine etherbase name
   node_settings.chaindata_env_config = {node_settings.data_directory->chaindata().path().string(), false, true, false, false, true};

   //  bool create{false};          // Whether db file must be created
   //  bool readonly{false};        // Whether db should be opened in RO mode
   //  bool exclusive{false};       // Whether this process has exclusive access
   //  bool inmemory{false};        // Whether this db is in memory
   //  bool shared{false};          // Whether this process opens a db already opened by another process
   //  bool read_ahead{false};      // Whether to enable mdbx read ahead
   //  bool write_map{false};       // Whether to enable mdbx write map

   node_settings.chaindata_env_config.max_readers = max_readers;
   node_settings.chain_config = config;

   silkrpc::DaemonSettings settings {
      node_settings.data_directory->chaindata().path().string(),
      http_port,
      "", 
      0 /* determine proper abi spec */,
      node_port,
      std::thread::hardware_concurrency() / 3,
      threads,
      static_cast<silkrpc::LogLevel>(verbosity), 
      silkrpc::WaitMode::blocking
   };

   my.reset(new rpc_plugin_impl(settings));

   SILK_INFO << "Initialized RPC Plugin";
}

void rpc_plugin::plugin_startup() {
   silkrpc::Daemon::run(my->settings, {"trust-evm-rpc", "version: "+appbase::app().full_version_string()});
}

void rpc_plugin::plugin_shutdown() {
}