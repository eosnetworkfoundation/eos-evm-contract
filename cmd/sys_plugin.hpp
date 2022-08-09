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
         appbase::app().shutdown();
      }

      static inline void error() {
         appbase::app().shutdown();
      }

      uint32_t get_verbosity();

      private:
         silkworm::log::Settings settings;
};