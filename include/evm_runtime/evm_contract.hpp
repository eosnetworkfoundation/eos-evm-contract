#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/singleton.hpp>
#include <eosio/ignore.hpp>

#include <evm_runtime/types.hpp>

#include <silkworm/core/types/block.hpp>
#include <silkworm/core/execution/processor.hpp>

#include <eosevm/block_mapping.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/block_info.hpp>
#endif

using namespace eosio;

namespace evm_runtime {

class [[eosio::contract]] evm_contract : public contract
{
public:
   using contract::contract;

   struct fee_parameters
   {
      std::optional<uint64_t> gas_price; ///< Minimum gas price (in 10^-18 EOS, aka wei) that is enforced on all
                                         ///< transactions. Required during initialization.

      std::optional<uint32_t> miner_cut; ///< Percentage cut (maximum allowed value of 100,000 which equals 100%) of the
                                         ///< gas fee collected for a transaction that is sent to the indicated miner of
                                         ///< that transaction. Required during initialization.

      std::optional<asset> ingress_bridge_fee; ///< Fee (in EOS) deducted from ingress transfers of EOS across bridge.
                                               ///< Symbol must be in EOS and quantity must be non-negative. If not
                                               ///< provided during initialization, the default fee of 0 will be used.
   };

   /**
    * @brief Initialize EVM contract
    *
    * @param chainid Chain ID of the EVM. Choose 17777 for a production network.
    *                For test networks, choose either 15555 for a public test network or 25555 for a local test
    * network.
    * @param fee_params See documentation of fee_parameters struct.
    */
   [[eosio::action]] void init(const uint64_t chainid, const fee_parameters& fee_params);

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

   [[eosio::action]] void pushtx(eosio::name miner, const bytes& rlptx);

   [[eosio::action]] void open(eosio::name owner);

   [[eosio::action]] void close(eosio::name owner);

   [[eosio::on_notify("*::transfer")]] void
   transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo);

   [[eosio::action]] void withdraw(eosio::name owner, eosio::asset quantity, const eosio::binary_extension<eosio::name> &to);

   /// @return true if all garbage has been collected
   [[eosio::action]] bool gc(uint32_t max);

   
   [[eosio::action]] void call(eosio::name from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit);
   [[eosio::action]] void admincall(const bytes& from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit);

   [[eosio::action]] void bridgereg(eosio::name receiver, eosio::name handler, const eosio::asset& min_fee);
   [[eosio::action]] void bridgeunreg(eosio::name receiver);

   [[eosio::action]] void assertnonce(eosio::name account, uint64_t next_nonce);

   [[eosio::action]] void setversion(uint64_t version);

   // Events
   [[eosio::action]] void evmtx(eosio::ignore<evm_runtime::evmtx_type> event){
      eosio::check(get_sender() == get_self(), "forbidden to call");
   };

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

   struct evm_version_type {
      struct pending {
         uint64_t version;
         block_timestamp time;

         bool is_active(time_point_sec genesis_time, block_timestamp current_time)const {
            eosevm::block_mapping bm(genesis_time.sec_since_epoch());
            auto current_block_num = bm.timestamp_to_evm_block_num(current_time.to_time_point().time_since_epoch().count());
            auto pending_block_num = bm.timestamp_to_evm_block_num(time.to_time_point().time_since_epoch().count());
            return current_block_num > pending_block_num;
         }
      };

      void promote_pending() {
         eosio::check(pending_version.has_value(), "no pending version");
         cached_version = pending_version.value().version;
         pending_version.reset();
      }

      std::optional<pending> pending_version;
      uint64_t               cached_version=0;
   };

   struct [[eosio::table]] [[eosio::contract("evm_contract")]] config
   {
      unsigned_int version; // placeholder for future variant index
      uint64_t chainid = 0;
      time_point_sec genesis_time;
      asset ingress_bridge_fee = asset(0, token_symbol);
      uint64_t gas_price = 0;
      uint32_t miner_cut = 0;
      uint32_t status = 0; // <- bit mask values from status_flags

      binary_extension<evm_version_type> evm_version;

      std::pair<uint64_t, bool> get_version(block_timestamp current_time) {
         bool update_required = false;
         uint64_t current_version = 0;
         if(evm_version.has_value()) {
            const auto& pending_version = evm_version.value().pending_version;
            if(pending_version.has_value()) {
               if( pending_version->is_active(genesis_time, current_time) ) {
                  update_required = true;
                  evm_version->promote_pending();
               }
            }
            current_version = evm_version->cached_version;
         }
         return std::make_pair(current_version, update_required);
      }

      void set_version(uint64_t new_version, block_timestamp current_time) {
         auto [current_version, _] = get_version(current_time);
         eosio::check(new_version > current_version, "new version must be greater than the active one");
         evm_version.emplace(evm_version_type{evm_version_type::pending{new_version, current_time},current_version});
      }

      EOSLIB_SERIALIZE(config, (version)(chainid)(genesis_time)(ingress_bridge_fee)(gas_price)(miner_cut)(status)(evm_version));
   };

private:
   enum class status_flags : uint32_t
   {
      frozen = 0x1
   };

   eosio::singleton<"config"_n, config> _config{get_self(), get_self().value};

   void assert_inited()
   {
      check(_config.exists(), "contract not initialized");
      check(_config.get().version == 0u, "unsupported configuration singleton");
   }

   void assert_unfrozen()
   {
      assert_inited();
      check((_config.get().status & static_cast<uint32_t>(status_flags::frozen)) == 0, "contract is frozen");
   }

   silkworm::Receipt execute_tx(eosio::name miner, silkworm::Block& block, silkworm::Transaction& tx, silkworm::ExecutionProcessor& ep, bool enforce_chain_id);
   void process_filtered_messages(const std::vector<silkworm::FilteredMessage>& filtered_messages);

   uint64_t get_and_increment_nonce(const name owner);

   checksum256 get_code_hash(name account) const;

   void handle_account_transfer(const eosio::asset& quantity, const std::string& memo);
   void handle_evm_transfer(eosio::asset quantity, const std::string& memo);

   void call_(intx::uint256 s, const bytes& to, intx::uint256 value, const bytes& data, uint64_t gas_limit, uint64_t nonce);

   // to allow sending through a Bytes (basic_string<uint8_t>) w/o copying over to a std::vector<char>
   void pushtx_bytes(eosio::name miner, const std::basic_string<uint8_t>& rlptx);
   using pushtx_action = eosio::action_wrapper<"pushtx"_n, &evm_contract::pushtx_bytes>;

   bool is_from_self = false;
   void push_tx(const silkworm::Transaction& txn, uint64_t current_version);
};


} // namespace evm_runtime

namespace std {
template <typename DataStream>
DataStream& operator<<(DataStream& ds, const std::basic_string<uint8_t>& bs)
{
   ds << (unsigned_int)bs.size();
   if (bs.size())
      ds.write((const char*)bs.data(), bs.size());
   return ds;
}
} // namespace std
