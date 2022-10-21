#pragma once

#include <appbase/application.hpp>
#include "block_conversion_plugin.hpp"
#include "engine_plugin.hpp"
#include "ship_receiver_plugin.hpp"
#include "sys_plugin.hpp"

class blockchain_plugin : public appbase::plugin<blockchain_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((sys_plugin)(engine_plugin)(block_conversion_plugin));
      blockchain_plugin();
      virtual ~blockchain_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class blockchain_plugin_impl> my;
};