#pragma once

#include <appbase/application.hpp>

#include <eosio/chain_conversions.hpp>
#include <eosio/chain_types.hpp>
#include <eosio/crypto.hpp>
#include <eosio/ship_protocol.hpp>

#include "logger_plugin.hpp"

#include <boost/beast/core/flat_buffer.hpp>

namespace channels {
   enum class fork_reason : uint8_t {
      network = 1,
      restart = 2,
      resync  = 3
   };

   struct fork {
      uint32_t    block_num;
      uint32_t    depth;
      fork_reason reason;
      uint32_t    lib;
   };

   using forks = appbase::channel_decl<struct forks_tag, std::shared_ptr<fork>>;

   struct block {
      uint32_t                                   block_num;
      eosio::checksum256                         block_id;
      uint32_t                                   lib;
      eosio::ship_protocol::signed_block_v0      block;
      std::shared_ptr<boost::beast::flat_buffer> data;
   };

   using blocks = appbase::channel_decl<struct blocks_tag, std::shared_ptr<block>>;
} // ns channels

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