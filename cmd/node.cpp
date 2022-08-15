#include <silkrpc/config.hpp>
#include <silkworm/common/log.hpp>

#include <iostream>
#include <string>

#include <appbase/application.hpp>

#include <trustevm_node/buildinfo.h>
#include <silkrpc/common/log.hpp>
#include <silkrpc/daemon.hpp>

#include "engine_plugin.hpp"
#include "ship_receiver_plugin.hpp"
#include "sys_plugin.hpp"
#include "blockchain_plugin.hpp"
#include "block_conversion_plugin.hpp"

std::string get_version_from_build_info() {
    const auto build_info{trustevm_node_get_buildinfo()};

    std::string application_version{"trustevm node version: "};
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

        if (!appbase::app().initialize<sys_plugin>(argc, argv))
           return -1;
        appbase::app().startup();
        appbase::app().set_thread_priority_max();
        appbase::app().exec();
    } catch( const std::runtime_error& err) {
        SILK_CRIT << "Error " << err.what();
    } catch( std::exception err ) {
        SILK_CRIT << "Error " << err.what();
    }
}
