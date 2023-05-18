/*
   Copyright 2020 The Silkrpc Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#include <iostream>
#include <string>

#include <appbase/application.hpp>
#include "rpc_plugin.hpp"
#include "sys_plugin.hpp"

#include <eos_evm_node/buildinfo.h>

std::string get_version_from_build_info() {
    const auto build_info{eos_evm_node_get_buildinfo()};

    std::string application_version{"EOS EVM RPC version: "};
    application_version.append(build_info->project_version);
    return application_version;
}

int main(int argc, char* argv[]) {
    try {
        appbase::app().set_version_string(get_version_from_build_info());
        appbase::app().register_plugin<sys_plugin>();
        appbase::app().register_plugin<rpc_plugin>();

        if (!appbase::app().initialize<sys_plugin, rpc_plugin>(argc, argv))
           return -1;
        appbase::app().startup();
        appbase::app().set_thread_priority_max();
        appbase::app().exec();
    } catch( const std::runtime_error& err) {
        SILK_CRIT << err.what();
    } catch( const std::exception &err ) {
        SILK_CRIT << err.what();
    }
}
