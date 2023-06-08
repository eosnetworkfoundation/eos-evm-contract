#pragma once

#include <appbase/application.hpp>

#include <eosio/chain_conversions.hpp>
#include <eosio/chain_types.hpp>
#include <eosio/crypto.hpp>
#include <eosio/ship_protocol.hpp>

#include "engine_plugin.hpp"
#include "sys_plugin.hpp"
#include "channels.hpp"

class ship_receiver_plugin : public appbase::plugin<ship_receiver_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((sys_plugin)(engine_plugin));
      ship_receiver_plugin();
      virtual ~ship_receiver_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

      std::optional<channels::native_block> get_start_from_block();

   private:
      std::unique_ptr<class ship_receiver_plugin_impl> my;
};