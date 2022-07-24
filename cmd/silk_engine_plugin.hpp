#pragma once

#include <appbase/application.hpp>
#include "logger_plugin.hpp"

class silk_engine_plugin : public appbase::plugin<silk_engine_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((logger_plugin));
      silk_engine_plugin();
      virtual ~silk_engine_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class silk_engine_plugin_impl> my;
};