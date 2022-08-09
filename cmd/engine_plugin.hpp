#pragma once

#include <appbase/application.hpp>
#include "sys_plugin.hpp"

#include <silkworm/common/settings.hpp>
#include <mdbx.h++>

class engine_plugin : public appbase::plugin<engine_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((sys_plugin));
      engine_plugin();
      virtual ~engine_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

      mdbx::env* get_db();
      silkworm::NodeSettings* get_node_settings();
      std::string get_address();
      uint32_t    get_threads();

   private:
      std::unique_ptr<class engine_plugin_impl> my;
};