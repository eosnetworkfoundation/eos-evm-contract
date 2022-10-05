#pragma once

#include <appbase/application.hpp>

#include <eosio/chain_conversions.hpp>
#include <eosio/chain_types.hpp>
#include <eosio/crypto.hpp>
#include <eosio/ship_protocol.hpp>

#include "engine_plugin.hpp"
#include "sys_plugin.hpp"
#include "chain_state.hpp"
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

      void update_core_chain_state(uint32_t block_num);
      void update_trust_chain_state(uint32_t block_num);
      chain_state* get_chain_state();

   private:
      std::unique_ptr<class ship_receiver_plugin_impl> my;
};