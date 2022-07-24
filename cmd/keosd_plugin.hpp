#pragma once

#include <appbase/application.hpp>

#include <silkworm/common/log.hpp>

class keosd_plugin : public appbase::plugin<keosd_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES();
      keosd_plugin() {}
      virtual ~keosd_plugin() {}
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

      private:
         silkworm::log::Settings settings;
};