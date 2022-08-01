#pragma once

#include <appbase/application.hpp>
#include "logger_plugin.hpp"

class core_system_plugin : public appbase::plugin<core_system_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((logger_plugin));
      core_system_plugin();
      virtual ~core_system_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class core_system_plugin_impl> my;
};