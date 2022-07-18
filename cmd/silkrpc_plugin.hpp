#pragma once

#include <appbase/application.hpp>

class silkrpc_plugin : public appbase::plugin<silkrpc_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES();
      silkrpc_plugin();
      virtual ~silkrpc_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class silkrpc_plugin_impl> my;
};