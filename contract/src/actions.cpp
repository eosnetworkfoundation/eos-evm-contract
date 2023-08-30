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

#ifdef WITH_LOGTIME
#define LOGTIME(MSG) eosio::internal_use_do_not_use::logtime(MSG)
#else
#define LOGTIME(MSG)
#endif

extern "C" {
__attribute__((eosio_wasm_import))
void set_action_return_value(void*, size_t);
}

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
    std::string err_msg = std::string(desc) + ": " + std::to_string(uint64_t(r));
    
    switch (r) {
        case ValidationResult::kWrongChainId:
            err_msg += " Wrong chain id";
            break;
        case ValidationResult::kUnsupportedTransactionType:
            err_msg += " Unsupported transaction type";
            break;
        case ValidationResult::kMaxFeeLessThanBase:
            err_msg += " Max fee per gas less than block base fee";
            break;
        case ValidationResult::kMaxPriorityFeeGreaterThanMax:
            err_msg += " Max priority fee per gas greater than max fee per gas";
            break;
        case ValidationResult::kInvalidSignature:
            err_msg += " Invalid signature";
            break;
        case ValidationResult::kIntrinsicGas:
            err_msg += " Intrinsic gas too low";
            break;
        case ValidationResult::kNonceTooHigh:
            err_msg += " Nonce too high";
            break;
        case ValidationResult::kMissingSender:
            err_msg += " Missing sender";
            break;
        case ValidationResult::kSenderNoEOA:
            err_msg += " Sender is not EOA";
            break;
        case ValidationResult::kWrongNonce:
            err_msg += " Wrong nonce";
            break;
        case ValidationResult::kInsufficientFunds:
            err_msg += " Insufficient funds";
            break;
        case ValidationResult::kBlockGasLimitExceeded:
            err_msg += " Block gas limit exceeded";
            break;
        default:
            break;
    }
    
    eosio::print(err_msg.c_str());
    eosio::check( false, std::move(err_msg));
}

Receipt evm_contract::execute_tx( eosio::name miner, Block& block, Transaction& tx, silkworm::ExecutionProcessor& ep, bool enforce_chain_id) {
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

    if(enforce_chain_id && !from_self) {
        check(tx.chain_id.has_value(), "tx without chain-id");
    }

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

void evm_contract::exec(const exec_input& input, const std::optional<exec_callback>& callback) {

    assert_unfrozen();

    const auto& current_config = _config.get();
    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(current_config.chainid);
    check( found_chain_config.has_value(), "failed to find expected chain config" );

    evm_common::block_mapping bm(current_config.genesis_time.sec_since_epoch());

    Block block;
    evm_common::prepare_block_header(block.header, bm, get_self().value,
        bm.timestamp_to_evm_block_num(eosio::current_time_point().time_since_epoch().count()));

    evm_runtime::state state{get_self(), get_self(), true, true};
    IntraBlockState ibstate{state};

    EVM evm{block, ibstate, *found_chain_config.value().second};

    Transaction txn;
    txn.to    = to_address(input.to);
    txn.data  = Bytes{input.data.begin(), input.data.end()};
    txn.from  = input.from.has_value()  ? to_address(input.from.value()) : evmc::address{};
    txn.value = input.value.has_value() ? to_uint256(input.value.value()) : 0;

    const CallResult vm_res{evm.execute(txn, 0x7ffffffffff)};

    exec_output output{
        .status  = int32_t(vm_res.status),
        .data    = bytes{vm_res.data.begin(), vm_res.data.end()},
        .context = input.context
    };

    if(callback.has_value()) {
        const auto& cb = callback.value();
        action(std::vector<permission_level>{}, cb.contract, cb.action, output
        ).send();
    } else {
        auto output_bin = eosio::pack(output);
        set_action_return_value(output_bin.data(), output_bin.size());
    }
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

    evm_runtime::state state{get_self(), get_self(), false, false};
    silkworm::ExecutionProcessor ep{block, engine, state, *found_chain_config->second};

    Transaction tx;
    ByteView bv{(const uint8_t*)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");
    LOGTIME("EVM TX DECODE");

    check(tx.max_priority_fee_per_gas == tx.max_fee_per_gas, "max_priority_fee_per_gas must be equal to max_fee_per_gas");
    check(tx.max_fee_per_gas >= current_config.gas_price, "gas price is too low");

    auto receipt = execute_tx(miner, block, tx, ep, true);

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
    require_auth(get_self());

    evm_runtime::state state{get_self(), eosio::same_payer};
    return state.gc(max);
}

} //evm_runtime
