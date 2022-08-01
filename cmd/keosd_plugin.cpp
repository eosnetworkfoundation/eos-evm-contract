#include "keosd_plugin.hpp"

#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>

void keosd_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("verbosity", boost::program_options::value<uint32_t>()->default_value(0),
        "verbosity level")
      ("stdout", boost::program_options::value<bool>()->default_value(false),
        "output to stdout instead of stderr")
      ("nocolor", boost::program_options::value<bool>()->default_value(false),
        "disable logging color")
      ("log-file", boost::program_options::value<std::string>()->default_value(""),
        "optionally tee logging into specified file")
   ;
}

void keosd_plugin::plugin_initialize( const appbase::variables_map& options ) {
   settings.log_verbosity = static_cast<silkworm::log::Level>(options.at("verbosity").as<uint32_t>());
   settings.log_std_out   = options.at("stdout").as<bool>();
   settings.log_nocolor   = options.at("nocolor").as<bool>();
   settings.log_file      = options.at("log-file").as<std::string>();

   silkworm::log::init(settings);

   SILK_DEBUG << "Initialized keosd Plugin";
}

void keosd_plugin::plugin_startup() {
}

void keosd_plugin::plugin_shutdown() {

}