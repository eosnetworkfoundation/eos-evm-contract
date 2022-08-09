#pragma once

#include <appbase/application.hpp>

#include <silkworm/types/block.hpp>
#include <eosio/chain_conversions.hpp>
#include <eosio/chain_types.hpp>
#include <eosio/crypto.hpp>
#include <eosio/ship_protocol.hpp>

#include <boost/beast/core/flat_buffer.hpp>

namespace channels {
   struct fork {
      uint32_t    block_num;
   };
   using forks = appbase::channel_decl<struct forks_tag, std::shared_ptr<fork>>;


   struct lib_change {
      uint32_t block_num;
   };
   using lib_advance = appbase::channel_decl<struct lib_advance_tag, std::shared_ptr<lib_change>>;

   struct native_action {
      uint32_t            ordinal;
      eosio::name         receiver;
      eosio::name         account;
      eosio::name         name;
      eosio::input_stream data;
   };
   
   struct native_trx {
      inline native_trx(eosio::checksum256 id, uint32_t cpu, int64_t elapsed)
         : id(id), cpu_usage_us(cpu), elapsed(elapsed) {}
      eosio::checksum256         id;
      uint32_t                   cpu_usage_us;
      int64_t                    elapsed;
      std::vector<native_action> actions;
   };

   struct native_block {
      native_block() = default;
      inline native_block(uint32_t bn, int64_t tm, bool sync = false)
        : block_num(bn), timestamp(tm), syncing(sync) {}
      uint32_t                block_num = 0;
      int64_t                 timestamp = 0;
      uint32_t                lib = 0;
      std::vector<native_trx> transactions;
      bool                    building = false;
      bool                    syncing  = false;
   };

   using native_blocks = appbase::channel_decl<struct blocks_tag, std::shared_ptr<native_block>>;

   using block = silkworm::Block;
   using blocks = appbase::channel_decl<struct blocks_tag, std::shared_ptr<block>>;
} // ns channels