#pragma once

#include <appbase/application.hpp>

#include <silkworm/common/log.hpp>

class sys_plugin : public appbase::plugin<sys_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES();
      sys_plugin(){}
      virtual ~sys_plugin() {}
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

      static inline void error(std::string msg) {
         SILK_CRIT << "System Error: " << msg;
         error();
      }

      static inline void error() {
         appbase::app().quit();
         //raise(SIGTERM);
         //raise(SIGINT);
      }

      silkworm::log::Level get_verbosity();

      private:
         silkworm::log::Settings settings;
};