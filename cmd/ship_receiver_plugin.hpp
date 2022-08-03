#pragma once

#include <appbase/application.hpp>

#include <eosio/chain_conversions.hpp>
#include <eosio/chain_types.hpp>
#include <eosio/crypto.hpp>
#include <eosio/ship_protocol.hpp>

#include "logger_plugin.hpp"
#include "channels.hpp"

class ship_receiver_plugin : public appbase::plugin<ship_receiver_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((logger_plugin));
      ship_receiver_plugin();
      virtual ~ship_receiver_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

   private:
      std::unique_ptr<class ship_receiver_plugin_impl> my;
};