#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/singleton.hpp>
#include <eosio/ignore.hpp>

#include <evm_runtime/types.hpp>
#include <evm_runtime/transaction.hpp>
#include <evm_runtime/runtime_config.hpp>
#include <silkworm/core/types/block.hpp>
#include <silkworm/core/execution/processor.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/block_info.hpp>
#endif

using namespace eosio;

namespace evm_runtime {

struct gas_prices_type;

class [[eosio::contract]] evm_contract : public contract
{
public:
   using contract::contract;
   evm_contract(eosio::name receiver, eosio::name code, const datastream<const char*>& ds);

   /**
    * @brief Initialize EVM contract
    *
    * @param chainid Chain ID of the EVM. Choose 17777 for a production network.
    *                For test networks, choose either 15555 for a public test network or 25555 for a local test
    * network.
    * @param fee_params See documentation of fee_parameters struct.
    */
   [[eosio::action]] void init(const uint64_t chainid, const fee_parameters& fee_params, eosio::binary_extension<eosio::name> token_contract);

   /**
    * @brief Change fee parameter values
    *
    * @param fee_params If a member of fee params is empty, the existing value of that parameter in state is not
    * changed.
    */
   [[eosio::action]] void setfeeparams(const fee_parameters& fee_params);

   [[eosio::action]] void addegress(const std::vector<name>& accounts);

   [[eosio::action]] void removeegress(const std::vector<name>& accounts);

   /**
    * @brief Freeze (or unfreeze) ability for user interaction with EVM contract.
    *
    * If the contract is in a frozen state, users are not able to deposit or withdraw tokens to/from the contract
    * whether via opening a balance and transferring into it, using the withdraw action, or using the EVM bridge.
    * Additionally, if the contract is in a frozen state, the pushtx action is rejected.
    *
    * @param value If true, puts the contract into a frozen state. If false, puts the contract into an unfrozen
    * state.
    */
   [[eosio::action]] void freeze(bool value);

   [[eosio::action]] void exec(const exec_input& input, const std::optional<exec_callback>& callback);

   [[eosio::action]] void pushtx(eosio::name miner, bytes rlptx, eosio::binary_extension<uint64_t> min_inclusion_price);

   [[eosio::action]] void open(eosio::name owner);

   [[eosio::action]] void close(eosio::name owner);

   [[eosio::on_notify("*::transfer")]] void
   transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo);

   [[eosio::action]] void withdraw(eosio::name owner, eosio::asset quantity, const eosio::binary_extension<eosio::name> &to);

   /// @return true if all garbage has been collected
   [[eosio::action]] bool gc(uint32_t max);

   
   [[eosio::action]] void call(eosio::name from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit);
   [[eosio::action]] void admincall(const bytes& from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit);
   [[eosio::action]] void callotherpay(eosio::name payer, eosio::name from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit);

   [[eosio::action]] void bridgereg(eosio::name receiver, eosio::name handler, const eosio::asset& min_fee);
   [[eosio::action]] void bridgeunreg(eosio::name receiver);

   [[eosio::action]] void assertnonce(eosio::name account, uint64_t next_nonce);

   [[eosio::action]] void setversion(uint64_t version);

   [[eosio::action]] void updtgasparam(eosio::asset ram_price_mb, uint64_t gas_price);
   [[eosio::action]] void setgasparam(uint64_t gas_txnewaccount, uint64_t gas_newaccount, uint64_t gas_txcreate, uint64_t gas_codedeposit, uint64_t gas_sset);

   [[eosio::action]] void setgasprices(const gas_prices_type& prices);
   [[eosio::action]] void setgaslimit(uint64_t ingress_gas_limit);

   [[eosio::action]] void swapgastoken(eosio::name new_token_contract, eosio::symbol new_symbol, eosio::name swap_dest_account, std::string swap_memo);
   [[eosio::action]] void migratebal(eosio::name from_name, int limit);

   // Events
   [[eosio::action]] void evmtx(eosio::ignore<evm_runtime::evmtx_type> event){
      eosio::check(get_sender() == get_self(), "forbidden to call");
   };

   // Events
   [[eosio::action]] void configchange(consensus_parameter_data_type consensus_parameter_data) {
      eosio::check(get_sender() == get_self(), "forbidden to call");
   };
   using configchange_action = eosio::action_wrapper<"configchange"_n, &evm_contract::configchange>;

#ifdef WITH_ADMIN_ACTIONS
   [[eosio::action]] void rmgcstore(uint64_t id);
   [[eosio::action]] void setkvstore(uint64_t account_id, const bytes& key, const std::optional<bytes>& value);
   [[eosio::action]] void rmaccount(uint64_t id);
   [[eosio::action]] void addevmbal(uint64_t id, const bytes& delta, bool subtract);
   [[eosio::action]] void addopenbal(name account, const bytes& delta, bool subtract);
   [[eosio::action]] void freezeaccnt(uint64_t id, bool value);
#endif

#ifdef WITH_TEST_ACTIONS
   [[eosio::action]] void testtx(const std::optional<bytes>& orlptx, const evm_runtime::test::block_info& bi);
   [[eosio::action]] void
   updatecode(const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code);
   [[eosio::action]] void updateaccnt(const bytes& address, const bytes& initial, const bytes& current);
   [[eosio::action]] void updatestore(
      const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current);
   [[eosio::action]] void dumpstorage(const bytes& addy);
   [[eosio::action]] void clearall();
   [[eosio::action]] void dumpall();
   [[eosio::action]] void setbal(const bytes& addy, const bytes& bal);
   [[eosio::action]] void testbaldust(const name test);
#endif

private:
   void open_internal_balance(eosio::name owner);
   std::shared_ptr<struct config_wrapper> _config;

   enum class status_flags : uint32_t
   {
      frozen = 0x1
   };

   void assert_inited();
   void assert_unfrozen();

   silkworm::Receipt execute_tx(const runtime_config& rc, eosio::name miner, silkworm::Block& block, const transaction& tx, silkworm::ExecutionProcessor& ep, const evmone::gas_parameters& gas_params);
   void process_filtered_messages(std::function<bool(const silkworm::FilteredMessage&)> extra_filter, const std::vector<silkworm::FilteredMessage>& filtered_messages);

   uint64_t get_and_increment_nonce(const name owner);

   checksum256 get_code_hash(name account) const;

   void handle_account_transfer(const eosio::asset& quantity, const std::string& memo);
   void handle_evm_transfer(eosio::asset quantity, const std::string& memo);

   void call_(const runtime_config& rc, intx::uint256 s, const bytes& to, intx::uint256 value, const bytes& data, uint64_t gas_limit, uint64_t nonce);

   using pushtx_action = eosio::action_wrapper<"pushtx"_n, &evm_contract::pushtx>;

   void process_tx(const runtime_config& rc, eosio::name miner, const transaction& tx, std::optional<uint64_t> min_inclusion_price);
   void dispatch_tx(const runtime_config& rc, const transaction& tx);

   uint64_t get_gas_price(uint64_t version);
   struct statistics get_statistics() const;
   void set_statistics(const struct statistics &v);
};

} // namespace evm_runtime

