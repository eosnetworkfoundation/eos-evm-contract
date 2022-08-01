#pragma once

#include <appbase/application.hpp>
#include "logger_plugin.hpp"

class block_conversion_plugin : public appbase::plugin<block_conversion_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((logger_plugin));
      block_conversion_plugin();
      virtual ~block_conversion_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class block_conversion_plugin_impl> my;
};