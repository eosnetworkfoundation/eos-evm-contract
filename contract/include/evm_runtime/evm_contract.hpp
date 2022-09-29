#include <eosio/eosio.hpp>
#include <evm_runtime/types.hpp>

#include <evm_runtime/state.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/block_info.hpp>
#endif

using namespace eosio;

namespace evm_runtime {

using namespace silkworm;

struct push_tx_result {
      db_stats        stats;
      uint64_t        gas_left{0};
      bytes           return_data;
};

CONTRACT evm_contract : public contract {
   public:
      using contract::contract;

      [[eosio::action]]
      push_tx_result pushtx(eosio::name ram_payer, const bytes& rlptx);

#ifdef WITH_TEST_ACTIONS
      ACTION testtx( const bytes& rlptx, const evm_runtime::test::block_info& bi );
      ACTION updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code);
    //  ACTION updateaccnt(const bytes& address, const bytes& initial, const bytes& current);
      ACTION updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current);
      ACTION dumpstorage(const bytes& addy);
      ACTION clearall();
      ACTION dumpall();
      ACTION setbal(const bytes& addy, const bytes& bal);
#endif

};


} //evm_runtime