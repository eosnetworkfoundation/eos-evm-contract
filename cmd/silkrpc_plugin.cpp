#include "silkrpc_plugin.hpp"

#include <silkrpc/config.hpp>

#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>

#include <silkrpc/buildinfo.h>
#include <silkrpc/common/log.hpp>
#include <silkrpc/daemon.hpp>

class silkrpc_plugin_impl : std::enable_shared_from_this<silkrpc_plugin_impl> {
   public:
      silkrpc_plugin_impl() {}

      void init(const silkrpc::DaemonSettings& s) {
         settings = s;
      }

   private:
      silkrpc::DaemonSettings settings;
};

silkrpc_plugin::silkrpc_plugin() : my(new silkrpc_plugin_impl) {}
silkrpc_plugin::~silkrpc_plugin() {}

void silkrpc_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("chain-data", boost::program_options::value<std::string>()->default_value("."),
        "chain data path as a string")
      ("http-port", boost::program_options::value<std::string>()->default_value("127.0.0.1:8080"),
        "http port for JSON RPC of the form <address>:<port>")
      ("contexts", boost::program_options::value<std::uint32_t>()->default_value(std::thread::hardware_concurrency() / 3),
        "number of I/O contexts")
      ("threads", boost::program_options::value<std::uint32_t>()->default_value(16),
        "number of worker threads")
   ;
}

void silkrpc_plugin::plugin_initialize( const appbase::variables_map& options ) {
   const auto& chain_data = options.at("chain-data").as<std::string>();
   const auto& http_port  = options.at("http-port").as<std::string>();
   const auto& contexts   = options.at("contexts").as<uint32_t>();
   const auto& threads    = options.at("threads").as<uint32_t>();

   silkrpc::DaemonSettings settings {
      chain_data,
      http_port,
      
   };
}

void silkrpc_plugin::plugin_startup() {
}

void silkrpc_plugin::plugin_shutdown() {

}

//ABSL_FLAG(std::string, chaindata, silkrpc::kEmptyChainData, "chain data path as string");
//ABSL_FLAG(std::string, http_port, silkrpc::kDefaultHttpPort, "Ethereum JSON RPC API local end-point as string <address>:<port>");
//ABSL_FLAG(std::string, engine_port, silkrpc::kDefaultEnginePort, "Engine JSON RPC API local end-point as string <address>:<port>");
//ABSL_FLAG(std::string, target, silkrpc::kDefaultTarget, "Erigon Core gRPC service location as string <address>:<port>");
//ABSL_FLAG(std::string, api_spec, silkrpc::kDefaultEth1ApiSpec, "JSON RPC API namespaces as comma-separated list of strings");
//ABSL_FLAG(uint32_t, num_contexts, std::thread::hardware_concurrency() / 3, "number of running I/O contexts as 32-bit integer");
//ABSL_FLAG(uint32_t, num_workers, 16, "number of worker threads as 32-bit integer");
//ABSL_FLAG(uint32_t, timeout, silkrpc::kDefaultTimeout.count(), "gRPC call timeout as 32-bit integer");
//ABSL_FLAG(silkrpc::LogLevel, log_verbosity, silkrpc::LogLevel::Critical, "logging verbosity level");
//ABSL_FLAG(silkrpc::WaitMode, wait_mode, silkrpc::WaitMode::blocking, "scheduler wait mode");

/*
silkrpc::DaemonSettings parse_args(int argc, char* argv[]) {
    absl::FlagsUsageConfig config;
    config.contains_helpshort_flags = [](absl::string_view) { return false; };
    config.contains_help_flags = [](absl::string_view filename) { return absl::EndsWith(filename, "main.cpp"); };
    config.contains_helppackage_flags = [](absl::string_view) { return false; };
    config.normalize_filename = [](absl::string_view f) { return std::string{f.substr(f.rfind("/") + 1)}; };
    config.version_string = []() { return get_version_from_build_info() + "\n"; };
    absl::SetFlagsUsageConfig(config);
    absl::SetProgramUsageMessage("C++ implementation of Ethereum JSON RPC API service within Thorax architecture");
    absl::ParseCommandLine(argc, argv);

    const silkrpc::DaemonSettings rpc_daemon_settings{
        absl::GetFlag(FLAGS_chaindata),
        absl::GetFlag(FLAGS_http_port),
        absl::GetFlag(FLAGS_engine_port),
        absl::GetFlag(FLAGS_api_spec),
        absl::GetFlag(FLAGS_target),
        absl::GetFlag(FLAGS_num_contexts),
        absl::GetFlag(FLAGS_num_workers),
        absl::GetFlag(FLAGS_log_verbosity),
        absl::GetFlag(FLAGS_wait_mode)
    };

    return rpc_daemon_settings;
}

int main(int argc, char* argv[]) {
    return silkrpc::Daemon::run(parse_args(argc, argv), {get_name_from_build_info(), get_library_versions()});
}
*/