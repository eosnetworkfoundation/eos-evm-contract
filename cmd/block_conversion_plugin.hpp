#pragma once

#include <appbase/application.hpp>
#include "sys_plugin.hpp"
#include "ship_receiver_plugin.hpp"

#include <eosio/name.hpp>

struct pushtx {
   eosio::name          miner;
   std::vector<uint8_t> rlpx;
};

EOSIO_REFLECT(pushtx, miner, rlpx)
class block_conversion_plugin : public appbase::plugin<block_conversion_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((sys_plugin)(ship_receiver_plugin)(engine_plugin));
      block_conversion_plugin();
      virtual ~block_conversion_plugin();
      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
      void plugin_initialize(const appbase::variables_map& options);
      void plugin_startup();
      void plugin_shutdown();

      uint32_t get_block_stride() const;

   private:
      std::unique_ptr<class block_conversion_plugin_impl> my;
};