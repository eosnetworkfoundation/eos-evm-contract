#pragma once

#include <appbase/application.hpp>
#include "sys_plugin.hpp"

class rpc_plugin : public appbase::plugin<rpc_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((sys_plugin));
      rpc_plugin();
      virtual ~rpc_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class rpc_plugin_impl> my;
};