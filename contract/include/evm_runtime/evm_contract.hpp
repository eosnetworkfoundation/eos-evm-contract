#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/block_info.hpp>
#endif

using namespace eosio;

namespace evm_runtime {

CONTRACT evm_contract : public contract {
   public:
      using contract::contract;

      [[eosio::action]]
      void init(const uint64_t chainid, const symbol& native_token_symbol);

      [[eosio::action]]
      void pushtx(eosio::name ram_payer, const bytes& rlptx);

      //evm_contract presents an eosio.token-like interface with exceptions described in comments below.
      // For all action parameters that are `asset` types, the symbol must be the same as defined in `init` action
      // No create action -- creation is done on `init` action
      [[eosio::action]]
      void issue(const name& to, const asset& quantity, const std::string& memo);
      [[eosio::action]]
      void retire(const asset& quantity, const std::string& memo);
      [[eosio::action]] 
      void open(const name& owner, const symbol& symbol, const name& ram_payer);
      [[eosio::action]]
      void close(const name& owner, const symbol& symbol);
      [[eosio::action]]
      // The `to` account must already have been opened
      void transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);

#ifdef WITH_TEST_ACTIONS
      ACTION testtx( const bytes& rlptx, const evm_runtime::test::block_info& bi );
      ACTION updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code);
      ACTION updateaccnt(const bytes& address, const bytes& initial, const bytes& current);
      ACTION updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current);
      ACTION dumpstorage(const bytes& addy);
      ACTION clearall();
      ACTION dumpall();
      ACTION setbal(const bytes& addy, const bytes& bal);
#endif
   private:
      struct [[eosio::table]] account {
         asset    balance;
         uint64_t dust = 0;

         uint64_t primary_key() const { return balance.symbol.code().raw(); }
      };

      struct [[eosio::table]] currency_stats {
         asset    supply;
         asset    max_supply;
         name     issuer;

         uint64_t primary_key() const { return supply.symbol.code().raw(); }
      };

      typedef eosio::multi_index<"accounts"_n, account> accounts;
      typedef eosio::multi_index<"stat"_n, currency_stats> stats;

      struct configuration {
         eosio::unsigned_int version; //placeholder for future variant index
         uint64_t chainid = 0;
         time_point_sec genesis_time;
      };
      EOSLIB_SERIALIZE(configuration, (version)(chainid)(genesis_time));

      eosio::singleton<"config"_n, configuration> _config{get_self(), get_self().value};

      void assert_inited() {
         check( _config.exists(), "contract not initialized" );
         check( _config.get().version == 0u, "unsupported configuration singleton" );
      }

      void sub_balance(const name& owner, const asset& value);
      void add_balance(const name& owner, const asset& value);
};


} //evm_runtime