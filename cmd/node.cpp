#include <silkrpc/config.hpp>
#include <silkworm/common/log.hpp>

#include <iostream>
#include <string>

#include <appbase/application.hpp>

#include <eos_evm_node/buildinfo.h>
#include <silkrpc/common/log.hpp>
//#include <silkrpc/daemon.hpp>

#include "engine_plugin.hpp"
#include "ship_receiver_plugin.hpp"
#include "sys_plugin.hpp"
#include "blockchain_plugin.hpp"
#include "block_conversion_plugin.hpp"
#include "rpc_plugin.hpp"

std::string get_version_from_build_info() {
    const auto build_info{eos_evm_node_get_buildinfo()};

    std::string application_version{"EOS EVM Node version: "};
    application_version.append(build_info->project_version);
    return application_version;
}

int main(int argc, char* argv[]) {
    try {
        appbase::app().set_version_string(get_version_from_build_info());
        appbase::app().register_plugin<blockchain_plugin>();
        appbase::app().register_plugin<engine_plugin>();
        appbase::app().register_plugin<ship_receiver_plugin>();
        appbase::app().register_plugin<sys_plugin>();
        appbase::app().register_plugin<block_conversion_plugin>();
        appbase::app().register_plugin<rpc_plugin>();

        if (!appbase::app().initialize<sys_plugin>(argc, argv))
           return -1;

        // if rpc_plugin enabled then enable blockchain_plugin
        if (appbase::app().get_plugin<rpc_plugin>().get_state() == appbase::abstract_plugin::initialized) {
           if (appbase::app().get_plugin<blockchain_plugin>().get_state() == appbase::abstract_plugin::registered) {
               appbase::app().get_plugin<blockchain_plugin>().initialize(appbase::app().get_options());
           }
        }

        appbase::app().startup();
        appbase::app().set_thread_priority_max();
        appbase::app().exec();
    } catch( const std::runtime_error& err) {
        SILK_CRIT << "Error " << err.what();
    } catch( std::exception err ) {
        SILK_CRIT << "Error " << err.what();
    }
}
