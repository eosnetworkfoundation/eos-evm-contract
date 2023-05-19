#define NDEBUG 1 // make sure assert is no-op in processor.cpp

#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/state.hpp>
#include <evm_runtime/intrinsics.hpp>
#include <evm_runtime/eosio.token.hpp>

#include <evm_common/block_mapping.hpp>

#include <silkworm/consensus/trust/engine.hpp>
// included here so NDEBUG is defined to disable assert macro
#include <silkworm/execution/processor.cpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/engine.hpp>
#include <evm_runtime/test/config.hpp>
#endif

#ifdef WITH_LOGTIME
#define LOGTIME(MSG) eosio::internal_use_do_not_use::logtime(MSG)
#else
#define LOGTIME(MSG)
#endif

namespace silkworm {
    // provide no-op bloom
    Bloom logs_bloom(const std::vector<Log>& logs) {
        return {};
    }
}

namespace evm_runtime {

static constexpr uint32_t hundred_percent = 100'000;

using namespace silkworm;

void set_fee_parameters(evm_contract::config& current_config,
                        const evm_contract::fee_parameters& fee_params,
                        bool allow_any_to_be_unspecified)
{
   if (fee_params.gas_price.has_value()) {
      current_config.gas_price = *fee_params.gas_price;
   } else {
      check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing gas_price");
   }

   if (fee_params.miner_cut.has_value()) {
      check(*fee_params.miner_cut <= hundred_percent, "miner_cut cannot exceed 100,000 (100%)");

      current_config.miner_cut = *fee_params.miner_cut;
   } else {
      check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing miner_cut");
   }

   if (fee_params.ingress_bridge_fee.has_value()) {
      check(fee_params.ingress_bridge_fee->symbol == token_symbol, "unexpected bridge symbol");
      check(fee_params.ingress_bridge_fee->amount >= 0, "ingress bridge fee cannot be negative");

      current_config.ingress_bridge_fee = *fee_params.ingress_bridge_fee;
   }
}

void evm_contract::init(const uint64_t chainid, const fee_parameters& fee_params)
{
   eosio::require_auth(get_self());

   check(!_config.exists(), "contract already initialized");
   check(!!lookup_known_chain(chainid), "unknown chainid");

   // Convert current time to EVM compatible block timestamp used as genesis time by rounding down to nearest second.
   time_point_sec genesis_time = eosio::current_time_point();

   config new_config = {
      .version = 0,
      .chainid = chainid,
      .genesis_time = genesis_time,
   };

   // Other fee parameters in new_config are still left at their (undesired) default values.
   // Correct those values now using the fee_params passed in as an argument to the init function.

   set_fee_parameters(new_config, fee_params, false); // enforce that all fee parameters are specified

   _config.set(new_config, get_self());

   inevm_singleton(get_self(), get_self().value).get_or_create(get_self());

   open(get_self());
}

void evm_contract::setfeeparams(const fee_parameters& fee_params)
{
   assert_inited();
   require_auth(get_self());

   config current_config = _config.get();
   set_fee_parameters(current_config, fee_params, true); // do not enforce that all fee parameters are specified
   _config.set(current_config, get_self());
}

void evm_contract::addegress(const std::vector<name>& accounts) {
    assert_inited();
    require_auth(get_self());

    egresslist egresslist_table(get_self(), get_self().value);

    for(const name& account : accounts)
        if(egresslist_table.find(account.value) == egresslist_table.end())
            egresslist_table.emplace(get_self(), [&](allowed_egress_account& a) {
                a.account = account;
            });
}

void evm_contract::removeegress(const std::vector<name>& accounts) {
    assert_inited();
    require_auth(get_self());

    egresslist egresslist_table(get_self(), get_self().value);

    for(const name& account : accounts)
        if(auto it = egresslist_table.find(account.value); it != egresslist_table.end())
            egresslist_table.erase(it);
}

void evm_contract::freeze(bool value) {
    eosio::require_auth(get_self());

    assert_inited();
    config config2 = _config.get();
    if (value) {
        config2.status |= static_cast<uint32_t>(status_flags::frozen);
    } else {
        config2.status &= ~static_cast<uint32_t>(status_flags::frozen);
    }
    _config.set(config2, get_self());
}

void check_result( ValidationResult r, const Transaction& txn, const char* desc ) {
    if( r == ValidationResult::kOk )
        return;

    if( r == ValidationResult::kMissingSender ) {
        eosio::print("txn.from.has_value is empty\n");
    } else if ( r == ValidationResult::kSenderNoEOA ) {
        eosio::print("get_code_hash is empty\n");
    } else if ( r == ValidationResult::kWrongNonce ) {
        eosio::print("invalid nonce:", txn.nonce, "\n");
    } else if ( r == ValidationResult::kInsufficientFunds ) {
        eosio::print("get_balance of from insufficient\n");
    } else if ( r == ValidationResult::kBlockGasLimitExceeded ) {
        eosio::print("available_gas\n");
    }

    eosio::print( "ERR: ", uint64_t(r), "\n" );
    eosio::check( false, desc );
}

Receipt evm_contract::execute_tx( eosio::name miner, Block& block, Transaction& tx, silkworm::ExecutionProcessor& ep ) {
    //when being called as an inline action, clutch in allowance for reserved addresses & signatures by setting from_self=true
    const bool from_self = get_sender() == get_self();

    balances balance_table(get_self(), get_self().value);

    if (miner == get_self()) {
        // If the miner is the contract itself, then there is no need to send the miner its cut.
        miner = {};
    }

    if (miner) {
        // Ensure the miner has a balance open early.
        balance_table.get(miner.value, "no balance open for miner");
    }

    bool deducted_miner_cut = false;

    std::optional<inevm_singleton> inevm;
    auto populate_bridge_accessors = [&]() {
        if(inevm)
            return;
        inevm.emplace(get_self(), get_self().value);
    };

    tx.from.reset();
    tx.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");
    LOGTIME("EVM RECOVER SENDER");

    if(from_self) {
        check(is_reserved_address(*tx.from), "actions from self without a reserved from address are unexpected");
        const name ingress_account(*extract_reserved_address(*tx.from));

        const intx::uint512 max_gas_cost = intx::uint256(tx.gas_limit) * tx.max_fee_per_gas;
        check(max_gas_cost + tx.value < std::numeric_limits<intx::uint256>::max(), "too much gas");
        const intx::uint256 value_with_max_gas = tx.value + (intx::uint256)max_gas_cost;

        populate_bridge_accessors();
        balance_table.modify(balance_table.get(ingress_account.value), eosio::same_payer, [&](balance& b){
            b.balance -= value_with_max_gas;
        });
        inevm->set(inevm->get() += value_with_max_gas, eosio::same_payer);

        ep.state().set_balance(*tx.from, value_with_max_gas);
        ep.state().set_nonce(*tx.from, tx.nonce);
    }
    else if(is_reserved_address(*tx.from))
        check(from_self, "bridge signature used outside of bridge transaction");

    ValidationResult r = consensus::pre_validate_transaction(tx, ep.evm().block().header.number, ep.evm().config(),
                                                             ep.evm().block().header.base_fee_per_gas);
    check_result( r, tx, "pre_validate_transaction error" );
    r = ep.validate_transaction(tx);
    check_result( r, tx, "validate_transaction error" );

    Receipt receipt;
    ep.execute_transaction(tx, receipt);

    // Calculate the miner portion of the actual gas fee (if necessary):
    std::optional<intx::uint256> gas_fee_miner_portion;
    if (miner) {
        uint64_t tx_gas_used = receipt.cumulative_gas_used; // Only transaction in the "block" so cumulative_gas_used is the tx gas_used.
        intx::uint512 gas_fee = intx::uint256(tx_gas_used) * tx.max_fee_per_gas;
        check(gas_fee < std::numeric_limits<intx::uint256>::max(), "too much gas");
        gas_fee *= _config.get().miner_cut;
        gas_fee /= hundred_percent;
        gas_fee_miner_portion.emplace(static_cast<intx::uint256>(gas_fee));
    }

    if(from_self)
        eosio::check(receipt.success, "ingress bridge actions must succeed");

    if(!ep.state().reserved_objects().empty()) {
        bool non_open_account_sent = false;
        intx::uint256 total_egress;
        populate_bridge_accessors();

        for(const auto& reserved_object : ep.state().reserved_objects()) {
            const evmc::address& address = reserved_object.first;
            const name egress_account(*extract_reserved_address(address));
            const Account& reserved_account = *reserved_object.second.current;

            check(reserved_account.code_hash == kEmptyHash, "contracts cannot be created in the reserved address space");
            check(egress_account.value != 0, "reserved 0 address cannot be used");

            if(reserved_account.balance ==  0_u256)
                continue;
            total_egress += reserved_account.balance;

            if(auto it = balance_table.find(egress_account.value); it != balance_table.end()) {
                balance_table.modify(balance_table.get(egress_account.value), eosio::same_payer, [&](balance& b){
                    b.balance += reserved_account.balance;
                    if (gas_fee_miner_portion.has_value() && egress_account == get_self()) {
                        check(!deducted_miner_cut, "unexpected error: contract account appears twice in reserved objects");
                        b.balance -= *gas_fee_miner_portion;
                        deducted_miner_cut = true;
                    }
                });
            }
            else {
                check(!non_open_account_sent, "only one non-open account for egress bridging allowed in single transaction");
                check(is_account(egress_account), "can only egress bridge to existing accounts");
                if(get_code_hash(egress_account) != checksum256())
                    egresslist(get_self(), get_self().value).get(egress_account.value, "non-open accounts containing contract code must be on allow list for egress bridging");

                check(reserved_account.balance % minimum_natively_representable == 0_u256, "egress bridging to non-open accounts must not contain dust");

                const bool was_to = tx.to && *tx.to == address;
                const Bytes exit_memo = {'E', 'V', 'M', ' ', 'e', 'x', 'i', 't'}; //yikes

                token::transfer_bytes_memo_action transfer_act(token_account, {{get_self(), "active"_n}});
                transfer_act.send(get_self(), egress_account, asset((uint64_t)(reserved_account.balance / minimum_natively_representable), token_symbol), was_to ? tx.data : exit_memo);

                non_open_account_sent = true;
            }
        }

        if(total_egress != 0_u256)
            inevm->set(inevm->get() -= total_egress, eosio::same_payer);
    }

    // Send miner portion of the gas fee, if any, to the balance of the miner:
    if (gas_fee_miner_portion.has_value() && *gas_fee_miner_portion != 0) {
        check(deducted_miner_cut, "unexpected error: contract account did not receive any funds through its reserved address");
        balance_table.modify(balance_table.get(miner.value), eosio::same_payer, [&](balance& b){
            b.balance += *gas_fee_miner_portion;
        });
    }

    LOGTIME("EVM EXECUTE");
    return receipt;
}

void evm_contract::pushtx( eosio::name miner, const bytes& rlptx ) {
    LOGTIME("EVM START");

    assert_unfrozen();

    eosio::check((get_sender() != get_self()) || (miner == get_self()),
                 "unexpected error: EVM contract generated inline pushtx without setting itself as the miner");

    const auto& current_config = _config.get();
    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(current_config.chainid);
    check( found_chain_config.has_value(), "failed to find expected chain config" );

    evm_common::block_mapping bm(current_config.genesis_time.sec_since_epoch());

    Block block;
    evm_common::prepare_block_header(block.header, bm, get_self().value, 
        bm.timestamp_to_evm_block_num(eosio::current_time_point().time_since_epoch().count()));

    silkworm::consensus::TrustEngine engine{*found_chain_config->second};

    evm_runtime::state state{get_self(), get_self()};
    silkworm::ExecutionProcessor ep{block, engine, state, *found_chain_config->second};

    Transaction tx;
    ByteView bv{(const uint8_t*)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");
    LOGTIME("EVM TX DECODE");

    check(tx.max_priority_fee_per_gas == tx.max_fee_per_gas, "max_priority_fee_per_gas must be equal to max_fee_per_gas");
    check(tx.max_fee_per_gas >= current_config.gas_price, "gas price is too low");

    auto receipt = execute_tx(miner, block, tx, ep);

    engine.finalize(ep.state(), ep.evm().block(), ep.evm().revision());
    ep.state().write_to_db(ep.evm().block().header.number);
}

void evm_contract::open(eosio::name owner) {
    assert_unfrozen();
    require_auth(owner);

    balances balance_table(get_self(), get_self().value);
    if(balance_table.find(owner.value) == balance_table.end())
        balance_table.emplace(owner, [&](balance& a) {
            a.owner = owner;
        });

    nextnonces nextnonce_table(get_self(), get_self().value);
    if(nextnonce_table.find(owner.value) == nextnonce_table.end())
        nextnonce_table.emplace(owner, [&](nextnonce& a) {
            a.owner = owner;
        });
}

void evm_contract::close(eosio::name owner) {
    assert_unfrozen();
    require_auth(owner);

    eosio::check(owner != get_self(), "Cannot close self");

    balances balance_table(get_self(), get_self().value);
    const balance& owner_account = balance_table.get(owner.value, "account is not open");

    eosio::check(owner_account.balance == balance_with_dust(), "cannot close because balance is not zero");
    balance_table.erase(owner_account);

    nextnonces nextnonce_table(get_self(), get_self().value);
    const nextnonce& next_nonce_for_owner = nextnonce_table.get(owner.value);
    //if the account has performed an EOS->EVM transfer the nonce needs to be maintained in case the account is re-opened in the future
    if(next_nonce_for_owner.next_nonce == 0)
        nextnonce_table.erase(next_nonce_for_owner);
}

uint64_t evm_contract::get_and_increment_nonce(const name owner) {
    nextnonces nextnonce_table(get_self(), get_self().value);

    const nextnonce& nonce = nextnonce_table.get(owner.value);
    uint64_t ret = nonce.next_nonce;
    nextnonce_table.modify(nonce, eosio::same_payer, [](nextnonce& n){
        ++n.next_nonce;
    });
    return ret;
}

checksum256 evm_contract::get_code_hash(name account) const {
    char buff[64];

    eosio::check(internal_use_do_not_use::get_code_hash(account.value, 0, buff, sizeof(buff)) <= sizeof(buff), "get_code_hash() too big");
    using start_of_code_hash_return = std::tuple<unsigned_int, uint64_t, checksum256>;
    const auto& [v, s, code_hash] = unpack<start_of_code_hash_return>(buff, sizeof(buff));

    return code_hash;
}

void evm_contract::handle_account_transfer(const eosio::asset& quantity, const std::string& memo) {
    eosio::name receiver(memo);

    balances balance_table(get_self(), get_self().value);
    const balance& receiver_account = balance_table.get(receiver.value, "receiving account has not been opened");

    balance_table.modify(receiver_account, eosio::same_payer, [&](balance& a) {
        a.balance.balance += quantity;
    });
}

void evm_contract::handle_evm_transfer(eosio::asset quantity, const std::string& memo) {
    //move all incoming quantity in to the contract's balance. the evm bridge trx will "pull" from this balance
    balances balance_table(get_self(), get_self().value);
    balance_table.modify(balance_table.get(get_self().value), eosio::same_payer, [&](balance& b){
        b.balance.balance += quantity;
    });

    const auto& current_config = _config.get();

    //subtract off the ingress bridge fee from the quantity that will be bridged
    quantity -= current_config.ingress_bridge_fee;
    eosio::check(quantity.amount > 0, "must bridge more than ingress bridge fee");

    const std::optional<Bytes> address_bytes = from_hex(memo);
    eosio::check(!!address_bytes, "unable to parse destination address");

    intx::uint256 value((uint64_t)quantity.amount);
    value *= minimum_natively_representable;

    const Transaction txn {
        .type = Transaction::Type::kLegacy,
        .nonce = get_and_increment_nonce(get_self()),
        .max_priority_fee_per_gas = current_config.gas_price,
        .max_fee_per_gas = current_config.gas_price,
        .gas_limit = 21000,
        .to = to_evmc_address(*address_bytes),
        .value = value,
        .r = 0u,  // r == 0 is pseudo signature that resolves to reserved address range
        .s = get_self().value
    };

    Bytes rlp;
    rlp::encode(rlp, txn);
    pushtx_action pushtx_act(get_self(), {{get_self(), "active"_n}});
    pushtx_act.send(get_self(), rlp);
}

void evm_contract::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo) {
    assert_unfrozen();
    eosio::check(get_code() == token_account && quantity.symbol == token_symbol, "received unexpected token");

    if(to != get_self() || from == get_self())
        return;

    if(memo.size() == 42 && memo[0] == '0' && memo[1] == 'x')
        handle_evm_transfer(quantity, memo);
    else if(!memo.empty() && memo.size() <= 13)
        handle_account_transfer(quantity, memo);
    else
        eosio::check(false, "memo must be either 0x EVM address or already opened account name to credit deposit to");
}

void evm_contract::withdraw(eosio::name owner, eosio::asset quantity, const eosio::binary_extension<eosio::name> &to) {
    assert_unfrozen();
    require_auth(owner);

    balances balance_table(get_self(), get_self().value);
    const balance& owner_account = balance_table.get(owner.value, "account is not open");

    check(owner_account.balance.balance.amount >= quantity.amount, "overdrawn balance");
    balance_table.modify(owner_account, eosio::same_payer, [&](balance& a) {
        a.balance.balance -= quantity;
    });

    token::transfer_action transfer_act(token_account, {{get_self(), "active"_n}});
    transfer_act.send(get_self(), to.has_value() ? *to : owner, quantity, std::string("Withdraw from EVM balance"));
}

bool evm_contract::gc(uint32_t max) {
    assert_unfrozen();

    evm_runtime::state state{get_self(), eosio::same_payer};
    return state.gc(max);
}

#ifdef WITH_TEST_ACTIONS
[[eosio::action]] void evm_contract::testtx( const std::optional<bytes>& orlptx, const evm_runtime::test::block_info& bi ) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    Block block;
    block.header = bi.get_block_header();

    evm_runtime::test::engine engine;
    evm_runtime::state state{get_self(), get_self()};
    silkworm::ExecutionProcessor ep{block, engine, state, evm_runtime::test::kTestNetwork};

    if(orlptx) {
        Transaction tx;
        ByteView bv{(const uint8_t*)orlptx->data(), orlptx->size()};
        eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");

        execute_tx(eosio::name{}, block, tx, ep);
    }
    engine.finalize(ep.state(), ep.evm().block(), ep.evm().revision());
    ep.state().write_to_db(ep.evm().block().header.number);
}

[[eosio::action]] void evm_contract::dumpstorage(const bytes& addy) {
    assert_inited();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(to_address(addy)));
    if(itr == inx.end()) {
        eosio::print("no data for: ");
        eosio::printhex(addy.data(), addy.size());
        eosio::print("\n");
        return;
    }

    eosio::print("storage: ");
    eosio::printhex(addy.data(), addy.size());

    uint64_t cnt=0;
    storage_table db(_self, itr->id);
    auto sitr = db.begin();
    while(sitr != db.end()) {
        eosio::print("\n");
        eosio::printhex(sitr->key.data(), sitr->key.size());
        eosio::print(":");
        eosio::printhex(sitr->value.data(), sitr->value.size());
        eosio::print("\n");
        ++sitr;
        ++cnt;
    }

    eosio::print(" = ", cnt, "\n");
}

[[eosio::action]] void evm_contract::dumpall() {
    assert_inited();

    eosio::require_auth(get_self());

    auto print_store = [](auto sitr) {
        eosio::print("    ");
        eosio::printhex(sitr->key.data(), sitr->key.size());
        eosio::print(":");
        eosio::printhex(sitr->value.data(), sitr->value.size());
        eosio::print("\n");
    };

    account_table accounts(_self, _self.value);
    auto itr = accounts.begin();
    eosio::print("DUMPALL start\n");
    while( itr != accounts.end() ) {
        eosio::print("  account:");
        eosio::printhex(itr->eth_address.data(), itr->eth_address.size());
        eosio::print("\n");
        storage_table db(_self, itr->id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            print_store( sitr );
            sitr++;
        }
        
        itr++;
    }
    eosio::print("  gc:");
    gc_store_table gc(_self, _self.value);
    auto i = gc.begin();
    while( i != gc.end() ) {
        eosio::print("   storage_id:");
        eosio::print(i->storage_id);
        eosio::print("\n");
        storage_table db(_self, i->storage_id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            print_store( sitr );
            ++sitr;
        }

        ++i;
    }

    eosio::print("DUMPALL end\n");
}


[[eosio::action]] void evm_contract::clearall() {
    assert_unfrozen();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto itr = accounts.begin();
    eosio::print("CLEAR start\n");
    while( itr != accounts.end() ) {
        eosio::print("  account:");
        eosio::printhex(itr->eth_address.data(), itr->eth_address.size());
        eosio::print("\n");
        storage_table db(_self, itr->id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            eosio::print("    ");
            eosio::printhex(sitr->key.data(), sitr->key.size());
            eosio::print(":");
            eosio::printhex(sitr->value.data(), sitr->value.size());
            eosio::print("\n");
            sitr = db.erase(sitr);
        }

        auto db_size = std::distance(db.cbegin(), db.cend());
        eosio::print("db size:", uint64_t(db_size), "\n");
        itr = accounts.erase(itr);
    }

    account_code_table codes(_self, _self.value);
    auto itrc = codes.begin();
    while(itrc != codes.end()) {
        itrc = codes.erase(itrc);
    }

    gc(std::numeric_limits<uint32_t>::max());

    auto account_size = std::distance(accounts.cbegin(), accounts.cend());
    eosio::print("accounts size:", uint64_t(account_size), "\n");

    eosio::print("CLEAR end\n");
}

[[eosio::action]] void evm_contract::updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto bvcode = ByteView{(const uint8_t *)code.data(), code.size()};
    state.update_account_code(to_address(address), incarnation, to_bytes32(code_hash), bvcode);
}

[[eosio::action]] void evm_contract::updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    eosio::print("updatestore: ");
    eosio::printhex(address.data(), address.size());
    eosio::print("\n   ");
    eosio::printhex(location.data(), location.size());
    eosio::print(":");
    eosio::printhex(current.data(), current.size());
    eosio::print("\n");
    
    state.update_storage(to_address(address), incarnation, to_bytes32(location), to_bytes32(initial), to_bytes32(current));
}

[[eosio::action]] void evm_contract::updateaccnt(const bytes& address, const bytes& initial, const bytes& current) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto maybe_account = [](const bytes& data) -> std::optional<Account> {
        std::optional<Account> res{};
        if(data.size()) {
            Account tmp;
            ByteView bv{(const uint8_t *)data.data(), data.size()};
            auto dec_res = Account::from_encoded_storage(bv);
            eosio::check(dec_res.second == DecodingResult::kOk, "unable to decode account");
            res = dec_res.first;
        }
        return res;
    };

    auto oinitial = maybe_account(initial);
    auto ocurrent = maybe_account(current);

    state.update_account(to_address(address), oinitial, ocurrent);
}

[[eosio::action]] void evm_contract::setbal(const bytes& addy, const bytes& bal) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(addy));

    if(itr == inx.end()) {
        accounts.emplace(get_self(), [&](auto& row){
            row.id = accounts.available_primary_key();;
            row.code_id = std::nullopt;
            row.eth_address = addy;
            row.balance = bal;
        });
    } else {
        accounts.modify(*itr, eosio::same_payer, [&](auto& row){
            row.balance = bal;
        });
    }
}

[[eosio::action]] void evm_contract::testbaldust(const name test) {
    if(test == "basic"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b +=                      200000000_u256; //adds to dust only
        b +=                           3000_u256; //adds to dust only
        b +=                100000000000000_u256; //adds strictly to balance
        b +=                200000000007000_u256; //adds to both balance and dust
        b +=                 60000000000000_u256; //adds to dust only
        b +=                 55000000000000_u256; //adds to dust only but dust overflows +1 to balance

        //expect:           415000200010000; .0004 EOS, 15000200010000 dust
        check(b.balance.amount == 4, "");
        check(b.dust == 15000200010000, "");

        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b -=                             45_u256; //substracts from dust only
        b -=                100000000000000_u256; //subtracts strictly from balance
        b -=                120000000000000_u256; //subtracts from both dust and balance, causes underflow on dust thus -1 balance

        //expect:           195000200009955; .0001 EOS, 95000200009955 dust
        check(b.balance.amount == 1, "");
        check(b.dust == 95000200009955, "");
    }
    else if(test == "underflow1"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b -=                             45_u256;
        //should fail with underflow on dust causing an underflow of balance
    }
    else if(test == "underflow2"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b -=                100000000000000_u256;
        //should fail with underflow on balance
    }
    else if(test == "underflow3"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b +=                200000000000000_u256;
        b -=                300000000000000_u256;
        //should fail with underflow on balance
    }
    else if(test == "underflow4"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b +=                          50000_u256;
        b -=                      500000000_u256;
        //should fail with underflow on dust causing an underflow of balance
    }
    else if(test == "underflow5"_n) {
        balance_with_dust b;
        // do a decrement that would overflow an int64_t but not uint64_t (for balance)
        //    ↱int64t max       ↱minimum EOS
        //    9223372036854775807 (2^63)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b +=                              50000_u256;
        b -=  100000000000000000000000000000000_u256;
        //should fail with underflow
    }
    else if(test == "overflow1"_n) {
        balance_with_dust b;
        // increment a value that would overflow a int64_t, but not uint64_t
        //    ↱int64t max       ↱minimum EOS
        //    9223372036854775807 (2^63)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b += 1000000000000000000000000000000000_u256;
        //should fail with overflow
    }
    else if(test == "overflow2"_n) {
        balance_with_dust b;
        // increment a value that would overflow a max_asset, but not an int64_t
        //    ↱max_asset max    ↱minimum EOS
        //    4611686018427387903 (2^62)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b +=  500000000000000000000000000000000_u256;
        //should fail with overflow
    }
    else if(test == "overflow3"_n || test == "overflow4"_n || test == "overflow5"_n || test == "overflowa"_n || test == "overflowb"_n || test == "overflowc"_n) {
        balance_with_dust b;
        // start with a value that should be the absolute max allowed
        //    ↱max_asset max    ↱minimum EOS
        //    4611686018427387903 (2^62)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b +=  461168601842738790399999999999999_u256;
        if(test == "overflow4"_n) {
            //add 1 to balance, should fail since it rolls balance over max_asset
            //                      ↱minimum EOS
            //                  .123456789abcdefghi EEOS
            b +=                    100000000000000_u256;
            //should fail with overflow
        }
        if(test == "overflow5"_n) {
            //add 1 to dust, causing a rollover making balance > max_asset
            //                      ↱minimum EOS
            //                  .123456789abcdefghi EEOS
            b +=                                  1_u256;
            //should fail with overflow
        }
        if(test == "overflowa"_n) {
            //add something huge
            //       ↱max_asset max    ↱minimum EOS
            //       4611686018427387903 (2^62)-1
            //       543210987654321̣123456789abcdefghi EEOS
            b +=  999461168601842738790399999999999999_u256;
            //should fail with overflow
        }
        if(test == "overflowb"_n) {
            // add max allowed to balance again; this should be a 2^62-1 + 2^62-1
            //    ↱max_asset max    ↱minimum EOS
            //    4611686018427387903 (2^62)-1
            //    543210987654321̣123456789abcdefghi EEOS
            b +=  461168601842738790300000000000000_u256;
            //should fail with overflow
        }
        if(test == "overflowc"_n) {
            // add max allowed to balance again; but also with max dust; should be a 2^62-1 + 2^62-1 + 1 on asset balance
            //    ↱max_asset max    ↱minimum EOS
            //    4611686018427387903 (2^62)-1
            //    543210987654321̣123456789abcdefghi EEOS
            b +=  461168601842738790399999999999999_u256;
            //should fail with overflow
        }
    }
    if(test == "overflowd"_n) {
        balance_with_dust b;
        //add something massive
        //            ↱max_asset max    ↱minimum EOS
        //            4611686018427387903 (2^62)-1
        //            543210987654321̣123456789abcdefghi EEOS
        b +=  99999999461168601842738790399999999999999_u256;
        //should fail with overflow
    }
}

#endif //WITH_TEST_ACTIONS

} //evm_runtime
