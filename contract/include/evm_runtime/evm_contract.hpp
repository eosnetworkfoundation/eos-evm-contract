#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>

#include <evm_runtime/types.hpp>

#include <silkworm/types/block.hpp>
#include <silkworm/consensus/engine.hpp>
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
      void setingressfee(asset ingress_bridge_fee);

      [[eosio::action]]
      void addegress(const std::vector<name>& accounts);

      [[eosio::action]]
      void removeegress(const std::vector<name>& accounts);

      [[eosio::action]]
      void freeze(bool value);

      [[eosio::action]]
      void pushtx(eosio::name ram_payer, const bytes& rlptx);

      [[eosio::action]]
      void open(eosio::name owner);

      [[eosio::action]]
      void close(eosio::name owner);

      [[eosio::on_notify(TOKEN_ACCOUNT_NAME "::transfer")]]
      void transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo);

      [[eosio::action]]
      void withdraw(eosio::name owner, eosio::asset quantity);
      
      /// @return true if all garbage has been collected
      [[eosio::action]]
      bool gc(uint32_t max);

#ifdef WITH_TEST_ACTIONS
      ACTION testtx( const bytes& rlptx, const evm_runtime::test::block_info& bi );
      ACTION updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code);
      ACTION updateaccnt(const bytes& address, const bytes& initial, const bytes& current);
      ACTION updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current);
      ACTION dumpstorage(const bytes& addy);
      ACTION clearall();
      ACTION dumpall();
      ACTION setbal(const bytes& addy, const bytes& bal);
      ACTION testbaldust(const name test);
#endif
   private:
      enum class status_flags : uint32_t {
         frozen = 0x1
      };

      struct [[eosio::table]] [[eosio::contract("evm_contract")]] config {
         unsigned_int   version;     //placeholder for future variant index
         uint64_t       chainid = 0;
         time_point_sec genesis_time;
         asset          ingress_bridge_fee = asset(0, token_symbol);
         uint32_t       status = 0; // <- bit mask values from status_flags

         EOSLIB_SERIALIZE(config, (version)(chainid)(genesis_time)(ingress_bridge_fee)(status));
      };

      eosio::singleton<"config"_n, config> _config{get_self(), get_self().value};

      void assert_inited() {
         check( _config.exists(), "contract not initialized" );
         check( _config.get().version == 0u, "unsupported configuration singleton" );
      }

      void assert_unfrozen() {
         assert_inited();
         check((_config.get().status & static_cast<uint32_t>(status_flags::frozen)) == 0, "contract is frozen");
      }

      void push_trx(eosio::name ram_payer, silkworm::Block& block, const bytes& rlptx, silkworm::consensus::IEngine& engine, const silkworm::ChainConfig& chain_config);

      uint64_t get_and_increment_nonce(const name owner);

      checksum256 get_code_hash(name account) const;

      void handle_account_transfer(const eosio::asset& quantity, const std::string& memo);
      void handle_evm_transfer(const eosio::asset& quantity, const std::string& memo);
};


} //evm_runtime