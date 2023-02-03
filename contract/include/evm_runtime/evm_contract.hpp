#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>

#include <evm_runtime/types.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/block_info.hpp>
#endif

using namespace eosio;

namespace evm_runtime {

CONTRACT evm_contract : public contract {
   public:
      using contract::contract;

      [[eosio::action]]
      void init(const uint64_t chainid);

      [[eosio::action]]
      void pushtx(eosio::name ram_payer, const bytes& rlptx);

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
      struct [[eosio::table]] [[eosio::contract("evm_contract")]] config {
         eosio::unsigned_int version; //placeholder for future variant index
         uint64_t chainid = 0;
         time_point_sec genesis_time;
      };
      EOSLIB_SERIALIZE(config, (version)(chainid)(genesis_time));

      eosio::singleton<"config"_n, config> _config{get_self(), get_self().value};

      void assert_inited() {
         check( _config.exists(), "contract not initialized" );
         check( _config.get().version == 0u, "unsupported configuration singleton" );
      }

      void push_trx(eosio::name ram_payer, silkworm::Block& block, const bytes& rlptx);
};


} //evm_runtime