#pragma once

#include <appbase/application.hpp>
#include "logger_plugin.hpp"
#include "silk_engine_plugin.hpp"
#include "receiver_channels.hpp"

class mock_receiver_plugin : public appbase::plugin<mock_receiver_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((logger_plugin));
      mock_receiver_plugin();
      virtual ~mock_receiver_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class mock_receiver_plugin_impl> my;
};