#include <eosio/eosio.hpp>

#include <evm_runtime/state.hpp>
#include <evm_runtime/types.hpp>
#include <tx_meta.hpp>
#include <record.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/block_info.hpp>
#endif

using namespace eosio;

namespace evm_runtime {

CONTRACT evm_contract : public contract {
   public:
      using contract::contract;

      [[eosio::action]]
      void init();

      [[eosio::action]]
      trust_evm::tx_meta pushtx(eosio::name ram_payer, const bytes& rlptx);

      [[eosio::action]]
      int64_t gentime();
      int64_t get_evm_block_number() const;



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
      struct internal_state {
         typedef multi_index< "internal"_n, ::trust_evm::record> state;

         static inline ::trust_evm::record get(const evm_contract* ec) {
            state s(ec->get_self(), ec->get_self().value);
            auto itr = s.find(0);
            return *itr;
         }

         static inline void set(const evm_contract* ec, const ::trust_evm::record& rec) {
            state s(ec->get_self(), ec->get_self().value);
            s.emplace(ec->get_self(), [&](auto& o) {
               o = rec;
            });
         }
      };
};


} //evm_runtime