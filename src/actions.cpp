#define NDEBUG 1 // make sure assert is no-op in processor.cpp

#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/state.hpp>
#include <evm_runtime/intrinsics.hpp>
#include <evm_runtime/eosio.token.hpp>
#include <evm_runtime/bridge.hpp>
#include <evm_runtime/config_wrapper.hpp>

#include <silkworm/core/protocol/trust_rule_set.hpp>
// included here so NDEBUG is defined to disable assert macro
#include <silkworm/core/execution/processor.hpp>

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

static constexpr char err_msg_invalid_addr[] = "invalid address";

using namespace silkworm;

evm_contract::evm_contract(eosio::name receiver, eosio::name code, const datastream<const char*>& ds) : 
        contract(receiver, code, ds), _config(std::make_shared<config_wrapper>(get_self())) {}

void evm_contract::assert_inited()
{
    check(_config->exists(), "contract not initialized");
    check(_config->get_version() == 0u, "unsupported configuration singleton");
}

void evm_contract::assert_unfrozen()
{
    assert_inited();
    check((_config->get_status() & static_cast<uint32_t>(status_flags::frozen)) == 0, "contract is frozen");
}

void evm_contract::init(const uint64_t chainid, const fee_parameters& fee_params, eosio::binary_extension<eosio::name> token_contract)
{
   eosio::require_auth(get_self());

   check(!_config->exists(), "contract already initialized");
   check(!!lookup_known_chain(chainid), "unknown chainid");

   // Convert current time to EVM compatible block timestamp used as genesis time by rounding down to nearest second.
   time_point_sec genesis_time = eosio::current_time_point();

   _config->set_version(0);
   _config->set_chainid(chainid);
   _config->set_genesis_time(genesis_time);

   // Other fee parameters in new_config are still left at their (undesired)
   // default values. Correct those values now using the fee_params passed in as
   // an argument to the init function.

   _config->set_fee_parameters(fee_params, false);  // enforce that all fee parameters are specified

   if (token_contract.has_value()) {
      _config->set_token_contract(*token_contract);
   }
   inevm_singleton(get_self(), get_self().value).get_or_create(get_self(), balance_with_dust {
      .balance = eosio::asset(0, fee_params.ingress_bridge_fee->symbol),
      .dust = 0
   });

   open_internal_balance(get_self());
}

void evm_contract::setfeeparams(const fee_parameters& fee_params)
{
   assert_inited();
   require_auth(get_self());

   _config->set_fee_parameters(
      fee_params,
      true);  // do not enforce that all fee parameters are specified
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
    auto status = _config->get_status();
    if (value) {
        status |= static_cast<uint32_t>(status_flags::frozen);
    } else {
        status &= ~static_cast<uint32_t>(status_flags::frozen);
    }
    _config->set_status(status);
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

Receipt evm_contract::execute_tx(const runtime_config& rc, eosio::name miner, Block& block, const transaction& txn, silkworm::ExecutionProcessor& ep, const evmone::gas_parameters& gas_params) {
    const auto& tx = txn.get_tx();
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
    bool from_self = false;

    std::optional<inevm_singleton> inevm;
    auto populate_bridge_accessors = [&]() {
        if(inevm)
            return;
        inevm.emplace(get_self(), get_self().value);
    };

    bool is_special_signature = silkworm::is_special_signature(tx.r, tx.s);

    ValidationResult r = silkworm::protocol::pre_validate_transaction(tx, ep.evm().revision(), ep.evm().config().chain_id,
                            ep.evm().block().header.base_fee_per_gas, ep.evm().block().header.data_gas_price(),
                            ep.evm().get_eos_evm_version(), gas_params);
    check_result( r, tx, "pre_validate_transaction error" );

    txn.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");
    LOGTIME("EVM RECOVER SENDER");

    // 1 For regular signature, it's impossible to from reserved address, 
    // and now we accpet them regardless from self or not, so no special treatment.
    // 2 For special signature, we will reject calls not from self 
    // and process the special balance if the tx is from reserved address.
    if (is_special_signature) {
        check(rc.allow_special_signature, "bridge signature used outside of bridge transaction");
        if (is_reserved_address(*tx.from)) {
            const name ingress_account(*extract_reserved_address(*tx.from));

            from_self = ingress_account == get_self();

            const intx::uint512 max_gas_cost = intx::uint256(tx.gas_limit) * tx.max_fee_per_gas;
            check(max_gas_cost + tx.value < std::numeric_limits<intx::uint256>::max(), "too much gas");
            const intx::uint256 value_with_max_gas = tx.value + (intx::uint256)max_gas_cost;

            populate_bridge_accessors();
            if (rc.gas_payer) {
                balance_table.modify(balance_table.get(rc.gas_payer->value, "gas payer account has not been opened"), eosio::same_payer, [&](balance& b){
                    b.balance -= (intx::uint256)max_gas_cost;
                });
                if (tx.value > 0) {
                    balance_table.modify(balance_table.get(ingress_account.value), eosio::same_payer, [&](balance& b){
                        b.balance -= tx.value;
                    });
                }
            }
            else {
                balance_table.modify(balance_table.get(ingress_account.value), eosio::same_payer, [&](balance& b){
                    b.balance -= value_with_max_gas;
                });
            }
           
            inevm->set(inevm->get() += value_with_max_gas, eosio::same_payer);

            ep.state().set_balance(*tx.from, value_with_max_gas);
            ep.state().set_nonce(*tx.from, tx.nonce);
        }
    }

    // A tx from self with regular signature can potentially from external source.
    // Therefore, only tx with special signature can waive the chain_id check.
    if (rc.enforce_chain_id) {
        check(tx.chain_id.has_value(), "tx without chain-id");
    }

    r = silkworm::protocol::validate_transaction(tx, ep.state(), ep.available_gas());
    check_result( r, tx, "validate_transaction error" );

    Receipt receipt;
    CallResult call_result; 
    const auto res = ep.execute_transaction(tx, receipt, gas_params, call_result);

    // Calculate the miner portion of the actual gas fee (if necessary):
    std::optional<intx::uint256> gas_fee_miner_portion;
    if (miner) {
        auto version = _config->get_evm_version();
        uint64_t tx_gas_used = receipt.cumulative_gas_used; // Only transaction in the "block" so cumulative_gas_used is the tx gas_used.
        if(version >= 3) {
            gas_fee_miner_portion.emplace(res.inclusion_fee);
        } else if(version >= 1) {
            eosio::check(ep.evm().block().header.base_fee_per_gas.has_value(), "no base fee");
            intx::uint512 gas_fee = intx::uint256(tx_gas_used) * tx.priority_fee_per_gas(ep.evm().block().header.base_fee_per_gas.value());
            check(gas_fee < std::numeric_limits<intx::uint256>::max(), "too much gas");
            gas_fee_miner_portion.emplace(static_cast<intx::uint256>(gas_fee));
        } else {
            intx::uint512 gas_fee = intx::uint256(tx_gas_used) * tx.max_fee_per_gas;
            check(gas_fee < std::numeric_limits<intx::uint256>::max(), "too much gas");
            gas_fee *= _config->get_miner_cut();
            gas_fee /= hundred_percent;
            gas_fee_miner_portion.emplace(static_cast<intx::uint256>(gas_fee));
        }
    }

    if (rc.abort_on_failure) {
        if (receipt.success == false) {
            size_t size = (int)call_result.data.length();
            constexpr size_t max_size = 1024;
            std::string errmsg;
            errmsg.reserve(max_size);
            errmsg += "inline evm tx failed, evmc_status_code:";
            errmsg += std::to_string((int)call_result.status);
            errmsg += ", data:[";
            errmsg += std::to_string(size);
            errmsg += "]";
            size_t i = 0;
            for (; i < size && errmsg.length() < max_size - 6; ++i ) {
                static const char hex_chars[] = "0123456789abcdef";
                errmsg += hex_chars[((uint8_t)call_result.data[i]) >> 4];
                errmsg += hex_chars[((uint8_t)call_result.data[i]) & 0xf];
            }
            if (i < size) {
                errmsg += "...";
            }
            eosio::check(false, errmsg);
        }
    }

    if(!ep.state().reserved_objects().empty()) {
        intx::uint256 total_egress;
        populate_bridge_accessors();

        intx::uint256 minimum_natively_representable = intx::uint256(_config->get_minimum_natively_representable());

        std::vector<bool> need_send;
        need_send.resize(ep.state().filtered_messages().size(), false);

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
                // check(!non_open_account_sent, "only one non-open account for egress bridging allowed in single transaction");
                // Assert not transfer to self
                check(egress_account != get_self(), "evm runtime account not open");
                check(is_account(egress_account), "can only egress bridge to existing accounts");
                if(get_code_hash(egress_account) != checksum256())
                    egresslist(get_self(), get_self().value).get(egress_account.value, "non-open accounts containing contract code must be on allow list for egress bridging");

                auto balance = reserved_account.balance;
                uint64_t pending_transfer = 0;
                for (size_t i = 0; i < ep.state().filtered_messages().size(); ++i) {      
                    const auto& rawmsg = ep.state().filtered_messages()[i];
                    if (rawmsg.receiver == address) {
                        check(++pending_transfer <= 5, "only five transfers to each non-open account allowed in single transaction");
                        auto value = intx::be::unsafe::load<uint256>(rawmsg.value.bytes);
                        check(value % minimum_natively_representable == 0_u256, "egress bridging to non-open accounts must not contain dust");
                        check(balance >= value, "sum of bridge transfers not match total received balance");
                        balance -= value;

                        // Only record action here so that we can launch transfers in order later.
                        need_send[i] = true;
                    }
                }

                check(balance == 0_u256, "sum of bridge transfers not match total received balance");
                // non_open_account_sent = true;
            }
        }

        // Keep the transfer order.
        for (size_t i = 0; i < ep.state().filtered_messages().size(); ++i) {
            if (!need_send[i]) {
                continue;
            }

            const auto& rawmsg = ep.state().filtered_messages()[i];
            const name egress_account(*extract_reserved_address(rawmsg.receiver));
            auto value = intx::be::unsafe::load<uint256>(rawmsg.value.bytes);
            token::transfer_bytes_memo_action transfer_act(_config->get_token_contract(), {{get_self(), "active"_n}});
            transfer_act.send(get_self(), egress_account, asset((uint64_t)(value / minimum_natively_representable), _config->get_token_symbol()), rawmsg.data);
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

    if (rc.gas_payer) {
        // return excess gas
        uint64_t tx_gas_used = receipt.cumulative_gas_used;
        auto base_fee = ep.evm().block().header.base_fee_per_gas.value();
        const intx::uint512 max_gas_cost = intx::uint256(tx.gas_limit) * tx.max_fee_per_gas;
        const intx::uint512 gas_fee = intx::uint256(tx_gas_used) * (tx.priority_fee_per_gas(base_fee) + base_fee);
        check(gas_fee < std::numeric_limits<intx::uint256>::max() && max_gas_cost < std::numeric_limits<intx::uint256>::max() && max_gas_cost > gas_fee, "invalid gas fee");
        const intx::uint256 refund = static_cast<intx::uint256>(max_gas_cost - gas_fee);

        const name ingress_account(*extract_reserved_address(*tx.from));

        balance_table.modify(balance_table.get(ingress_account.value), eosio::same_payer, [&](balance& b){
            b.balance -= refund;
        });
        balance_table.modify(balance_table.get(rc.gas_payer->value), eosio::same_payer, [&](balance& b){
            b.balance += refund;
        });
        // inevm remain same
    }

    // Statistics
    if (!from_self) {
        // Gas income from tx sent from self should not be counted.
        // Bridge transfers can generate such txs.
        uint64_t tx_gas_used = receipt.cumulative_gas_used; // Only transaction in the "block" so cumulative_gas_used is the tx gas_used.
        auto s = get_statistics();
        if (_config->get_evm_version() >= 1) {
            intx::uint512 gas_fee = intx::uint256(tx_gas_used) * ep.evm().block().header.base_fee_per_gas.value();
            check(gas_fee < std::numeric_limits<intx::uint256>::max(), "too much gas");
            s.gas_fee_income += static_cast<intx::uint256>(gas_fee);
        } else {
            intx::uint512 gas_fee = intx::uint256(tx_gas_used) * tx.max_fee_per_gas;
            check(gas_fee < std::numeric_limits<intx::uint256>::max(), "too much gas");
            if (gas_fee_miner_portion.has_value()) {
                gas_fee -= *gas_fee_miner_portion;
            } 
            s.gas_fee_income += static_cast<intx::uint256>(gas_fee);
        }
        set_statistics(s);
    }

    LOGTIME("EVM EXECUTE");
    return receipt;
}

void evm_contract::exec(const exec_input& input, const std::optional<exec_callback>& callback) {

    assert_unfrozen();

    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(_config->get_chainid());
    check( found_chain_config.has_value(), "unknown chainid" );

    eosevm::block_mapping bm(_config->get_genesis_time().sec_since_epoch());

    Block block;
    auto evm_version = _config->get_evm_version();

    std::optional<uint64_t> base_fee_per_gas;
    if (evm_version >= 1) {
        base_fee_per_gas = get_gas_price(evm_version);
    }

    eosevm::prepare_block_header(block.header, bm, get_self().value,
        bm.timestamp_to_evm_block_num(eosio::current_time_point().time_since_epoch().count()), evm_version, base_fee_per_gas);

    evm_runtime::state state{get_self(), get_self(), true};
    IntraBlockState ibstate{state};

    const auto& consensus_param = _config->get_consensus_param();

    auto gas_params = std::visit([&](const auto &v) {
        return evmone::gas_parameters(
            v.gas_parameter.gas_txnewaccount,
            v.gas_parameter.gas_newaccount,
            v.gas_parameter.gas_txcreate,
            v.gas_parameter.gas_codedeposit,
            v.gas_parameter.gas_sset
        );
    }, consensus_param);

    EVM evm{block, ibstate, *found_chain_config.value().second};

    Transaction txn;
    txn.to    = to_address(input.to);
    txn.data  = Bytes{input.data.begin(), input.data.end()};
    txn.from  = input.from.has_value()  ? to_address(input.from.value()) : evmc::address{};
    txn.value = input.value.has_value() ? to_uint256(input.value.value()) : 0;

    const CallResult vm_res{evm.execute(txn, 0x7ffffffffff, gas_params)};

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

void evm_contract::process_filtered_messages(std::function<bool(const silkworm::FilteredMessage&)> extra_filter, const std::vector<silkworm::FilteredMessage>& filtered_messages ) {

    intx::uint256 accumulated_value;
    for(const auto& rawmsg : filtered_messages) {
        if (!(extra_filter)(rawmsg)) {
            continue;
        }

        auto msg = bridge::decode_message(ByteView{rawmsg.data});
        eosio::check(msg.has_value(), "unable to decode bridge message");

        auto& msg_v0 = std::get<bridge::message_v0>(msg.value());

        const auto& receiver = msg_v0.get_account_as_name();
        eosio::check(eosio::is_account(receiver), "receiver is not account");

        message_receiver_table message_receivers(get_self(), get_self().value);
        auto it = message_receivers.find(receiver.value);
        eosio::check(it != message_receivers.end(), "receiver not registered");

        eosio::check(msg_v0.force_atomic == false || it->has_flag(message_receiver::FORCE_ATOMIC), "unable to process message");

        intx::uint256 min_fee((uint64_t)it->min_fee.amount);
        min_fee *= intx::uint256(_config->get_minimum_natively_representable());

        auto value = intx::be::unsafe::load<uint256>(rawmsg.value.bytes);
        eosio::check(value >= min_fee, "min_fee not covered");

        balances balance_table(get_self(), get_self().value);
        const balance& receiver_account = balance_table.get(receiver.value, "receiver account is not open");

        action(std::vector<permission_level>{}, it->handler, "onbridgemsg"_n,
            bridge_message{ bridge_message_v0 {
                .receiver  = msg_v0.get_account_as_name(),
                .sender    = to_bytes(rawmsg.sender),
                .timestamp = eosio::current_time_point(),
                .value     = to_bytes(rawmsg.value),
                .data      = msg_v0.data
            } }
        ).send();

        balance_table.modify(receiver_account, eosio::same_payer, [&](balance& row) {
            row.balance += value;
        });

        accumulated_value += value;
    }

    if(accumulated_value > 0) {
        balances balance_table(get_self(), get_self().value);
        const balance& self_balance = balance_table.get(get_self().value);
        balance_table.modify(self_balance, eosio::same_payer, [&](balance& row) {
            row.balance -= accumulated_value;
        });
    }

}

void evm_contract::process_tx(const runtime_config& rc, eosio::name miner, const transaction& txn, std::optional<uint64_t> min_inclusion_price) {
    LOGTIME("EVM START1");

    const auto& tx = txn.get_tx();
    eosio::check(rc.allow_non_self_miner || miner == get_self(),
                 "unexpected error: EVM contract generated inline pushtx without setting itself as the miner");

    auto current_version = _config->get_evm_version_and_maybe_promote();

    auto gas_param_pair = _config->get_consensus_param_and_maybe_promote();
    if (gas_param_pair.second) {
        // should not happen
        eosio::check(current_version >= 1, "gas param change requires evm_version >= 1");
    }

    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(_config->get_chainid());
    check( found_chain_config.has_value(), "unknown chainid" );

    eosevm::block_mapping bm(_config->get_genesis_time().sec_since_epoch());

    Block block;

    std::optional<uint64_t> base_fee_per_gas;
    if (current_version >= 1) {
        base_fee_per_gas = get_gas_price(current_version);
    }

    eosevm::prepare_block_header(block.header, bm, get_self().value,
        bm.timestamp_to_evm_block_num(eosio::current_time_point().time_since_epoch().count()), current_version, base_fee_per_gas);

    silkworm::protocol::TrustRuleSet engine{*found_chain_config->second};

    evm_runtime::state state{get_self(), get_self(), false, false};

    auto gas_params = std::visit([&](const auto &v) {
        return evmone::gas_parameters(
            v.gas_parameter.gas_txnewaccount,
            v.gas_parameter.gas_newaccount,
            v.gas_parameter.gas_txcreate,
            v.gas_parameter.gas_codedeposit,
            v.gas_parameter.gas_sset
        );
    }, gas_param_pair.first);

    if (current_version >= 1) {
        auto inclusion_price = std::min(tx.max_priority_fee_per_gas, tx.max_fee_per_gas - *base_fee_per_gas);
        eosio::check(inclusion_price >= (min_inclusion_price.has_value() ? *min_inclusion_price : 0), "inclusion price must >= min_inclusion_price");
    } else { // old behavior
        check(tx.max_priority_fee_per_gas == tx.max_fee_per_gas, "max_priority_fee_per_gas must be equal to max_fee_per_gas");
        check(tx.max_fee_per_gas >= get_gas_price(current_version), "gas price is too low");
    }

    auto gas_prices = _config->get_gas_prices();
    auto gp = silkworm::gas_prices_t{gas_prices.overhead_price.value_or(0), gas_prices.storage_price.value_or(0)};
    silkworm::ExecutionProcessor ep{block, engine, state, *found_chain_config->second, gp};

    // Capture all messages to reserved addresses. They should be bridge transfers and EVM messages.
    ep.set_evm_message_filter([&](const evmc_message& message) -> bool {
        return is_reserved_address(message.recipient);
    });

    auto receipt = execute_tx(rc, miner, block, txn, ep, gas_params);

    // Filter EVM messages (with data) that are sent to the reserved address
    // corresponding to the EOS account holding the contract (self)
    process_filtered_messages([&](const silkworm::FilteredMessage& message) -> bool {
        static auto me = make_reserved_address(get_self().value);
        return message.receiver == me && message.data.size() > 0;
    }, ep.state().filtered_messages());

    engine.finalize(ep.state(), ep.evm().block());
    ep.state().write_to_db(ep.evm().block().header.number);

    if (gas_param_pair.second) {
        configchange_action act{get_self(), std::vector<eosio::permission_level>()};
        act.send(gas_param_pair.first);
    }

    if(current_version >= 3) {
        auto event = evmtx_type{evmtx_v3{current_version, txn.get_rlptx(), gas_prices.overhead_price.value_or(0), gas_prices.storage_price.value_or(0)}};
        action(std::vector<permission_level>{}, get_self(), "evmtx"_n, event).send();
    } else if (current_version >= 1) {
        auto event = evmtx_type{evmtx_v1{current_version, txn.get_rlptx(), *base_fee_per_gas}};
        action(std::vector<permission_level>{}, get_self(), "evmtx"_n, event).send();
    }
    LOGTIME("EVM END");
}

void evm_contract::pushtx(eosio::name miner, bytes rlptx, eosio::binary_extension<uint64_t> min_inclusion_price) {
    LOGTIME("EVM START0");
    assert_unfrozen();

    auto evm_version = _config->get_evm_version();
    if (evm_version >= 1) _config->process_price_queue();

    // Use default runtime configuration parameters.
    runtime_config rc;

    // Check if the transaction is initiated by the contract itself.
    // When the contract calls this as an inline action, it implies a special
    // context where certain rules are relaxed or altered.
    // 1. Allowance for special signatures: This permits the use of special
    // signatures that are otherwise restricted.
    // 2. Enforce immediate EOS transaction failure on error: In case of failure
    // in the EVM transaction execution, the EOS transaction is set to abort
    // immediately.
    // 3. Disable chainId enforcement: This is applicable because the RLP
    // transaction sent by the contract is a legacy pre-EIP155 transaction that is
    // not replay-protected (lacks a chain ID).
    // 4. Restrict miner to self: Ensures that if the transaction originates from
    // the contract then the miner must also be the contract itself.
    if (get_sender() == get_self()) {
        rc.allow_special_signature = true;
        rc.abort_on_failure = true;
        rc.enforce_chain_id = false;
        rc.allow_non_self_miner = false;
    }

    std::optional<uint64_t> min_inclusion_price_;
    if (min_inclusion_price.has_value()) {
        min_inclusion_price_ = *min_inclusion_price;
        check(evm_version >= 1, "min_inclusion_price requires evm_version >= 1");
    }

    process_tx(rc, miner, transaction{std::move(rlptx)}, min_inclusion_price_);
}

void evm_contract::open(eosio::name owner) {
    assert_unfrozen();
    require_auth(owner);
    open_internal_balance(owner);
}

void evm_contract::open_internal_balance(eosio::name owner) {
    balances balance_table(get_self(), get_self().value);
    if(balance_table.find(owner.value) == balance_table.end())
        balance_table.emplace(owner, [&](balance& a) {
            a.owner = owner;
            a.balance.balance = eosio::asset(0, _config->get_token_symbol());
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

    eosio::check(owner_account.balance.is_zero(), "cannot close because balance is not zero");
    balance_table.erase(owner_account);

    nextnonces nextnonce_table(get_self(), get_self().value);
    const nextnonce& next_nonce_for_owner = nextnonce_table.get(owner.value);
    //if the account has performed an EOS->EVM transfer the nonce needs to be maintained in case the account is re-opened in the future
    if(next_nonce_for_owner.next_nonce == 0)
        nextnonce_table.erase(next_nonce_for_owner);
}

uint64_t evm_contract::get_and_increment_nonce(const name owner) {
    nextnonces nextnonce_table(get_self(), get_self().value);

    const nextnonce& nonce = nextnonce_table.get(owner.value, "caller account has not been opened");
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
    if(_config->get_evm_version() >= 1) _config->process_price_queue();
    eosio::name receiver(memo);

    balances balance_table(get_self(), get_self().value);
    const balance& receiver_account = balance_table.get(receiver.value, "receiving account has not been opened");

    balance_table.modify(receiver_account, eosio::same_payer, [&](balance& a) {
        a.balance.balance += quantity;
    });
}

void evm_contract::handle_evm_transfer(eosio::asset quantity, const std::string& memo) {
    auto current_version = _config->get_evm_version();
    if(current_version >= 1) _config->process_price_queue();
    //move all incoming quantity in to the contract's balance. the evm bridge trx will "pull" from this balance
    balances balance_table(get_self(), get_self().value);
    balance_table.modify(balance_table.get(get_self().value), eosio::same_payer, [&](balance& b){
        b.balance.balance += quantity;
    });

    //subtract off the ingress bridge fee from the quantity that will be bridged
    quantity -= _config->get_ingress_bridge_fee();
    eosio::check(quantity.amount > 0, "must bridge more than ingress bridge fee");

    // Statistics
    auto s = get_statistics();
    s.ingress_bridge_fee_income.balance += _config->get_ingress_bridge_fee();
    set_statistics(s);

    const std::optional<Bytes> address_bytes = from_hex(memo);
    eosio::check(!!address_bytes, "unable to parse destination address");

    intx::uint256 value((uint64_t)quantity.amount);
    value *= intx::uint256(_config->get_minimum_natively_representable());

    auto calculate_gas_limit = [&](const evmc::address& destination) -> int64_t {
        int64_t gas_limit = _config->get_ingress_gas_limit();

        account_table accounts(get_self(), get_self().value);
        auto inx = accounts.get_index<"by.address"_n>();
        auto itr = inx.find(make_key(destination));

        if(itr == inx.end()) {
            gas_limit += std::visit([&](const auto &v) { return v.gas_parameter.gas_txnewaccount; }, _config->get_consensus_param());
        }

        return gas_limit;
    };

    auto txn_price = get_gas_price(current_version);

    Transaction txn;
    txn.type = TransactionType::kLegacy;
    txn.nonce = get_and_increment_nonce(get_self());
    txn.max_priority_fee_per_gas = txn_price;
    txn.max_fee_per_gas = txn_price;
    txn.to = to_evmc_address(*address_bytes);
    txn.gas_limit = calculate_gas_limit(*txn.to);
    txn.value = value;
    txn.r = 0u;  // r == 0 is pseudo signature that resolves to reserved address range
    txn.s = get_self().value;

    runtime_config rc;
    rc.allow_special_signature = true;
    rc.abort_on_failure = true;
    rc.enforce_chain_id = false;
    rc.allow_non_self_miner = false;

    dispatch_tx(rc, transaction{std::move(txn)});
}

void evm_contract::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo) {
    assert_unfrozen();
    
    // Allow transfer non-EOS tokens out.
    if(to != get_self() || from == get_self())
        return;

    eosio::check(get_code() == _config->get_token_contract() && quantity.symbol == _config->get_token_symbol(), "received unexpected token");

    // special case for gas token swap
    if (memo.size() == 0 && from == _config->get_token_contract()) {
        return;
    }

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

    check(_config->get_token_symbol() == quantity.symbol, "invalid symbol");
    check(owner_account.balance.balance.amount >= quantity.amount, "overdrawn balance");
    balance_table.modify(owner_account, eosio::same_payer, [&](balance& a) {
        a.balance.balance.symbol = _config->get_token_symbol();
        a.balance.balance -= quantity;
    });

    token::transfer_action transfer_act(_config->get_token_contract(), {{get_self(), "active"_n}});
    transfer_act.send(get_self(), to.has_value() ? *to : owner, quantity, std::string("Withdraw from EVM balance"));
}

bool evm_contract::gc(uint32_t max) {
    assert_unfrozen();
    require_auth(get_self());

    evm_runtime::state state{get_self(), eosio::same_payer};
    return state.gc(max);
}

void evm_contract::call_(const runtime_config& rc, intx::uint256 s, const bytes& to, intx::uint256 value, const bytes& data, uint64_t gas_limit, uint64_t nonce) {
    auto current_version = _config->get_evm_version();
    if(current_version >= 1) _config->process_price_queue();

    auto txn_price = get_gas_price(current_version);

    Transaction txn;
    txn.type = TransactionType::kLegacy;
    txn.nonce = nonce;
    txn.max_priority_fee_per_gas = txn_price;
    txn.max_fee_per_gas = txn_price;
    txn.gas_limit = gas_limit;
    txn.value = value;
    txn.data = Bytes{(const uint8_t*)data.data(), data.size()};
    txn.r = 0u;  // r == 0 is pseudo signature that resolves to reserved address range
    txn.s = s;

    // Allow empty to so that it can support contract creation calls.
    if (!to.empty()) {
        ByteView bv_to{(const uint8_t*)to.data(), to.size()};
        txn.to = to_evmc_address(bv_to);
    }

    dispatch_tx(rc, transaction{std::move(txn)});
}

void evm_contract::dispatch_tx(const runtime_config& rc, const transaction& tx) {
    if (_config->get_evm_version_and_maybe_promote() >= 1) {
        process_tx(rc, get_self(), tx, {} /* min_inclusion_price */);
    } else {
        eosio::check(!rc.gas_payer && rc.allow_special_signature && rc.abort_on_failure && !rc.enforce_chain_id && !rc.allow_non_self_miner, "invalid runtime config");
        action(permission_level{get_self(),"active"_n}, get_self(), "pushtx"_n,
            std::tuple<eosio::name, bytes>(get_self(), tx.get_rlptx())
        ).send();
    }
}

void evm_contract::call(eosio::name from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit) {
    assert_unfrozen();
    require_auth(from);

    // Prepare v
    eosio::check(value.size() == sizeof(intx::uint256), "invalid value");
    intx::uint256 v = intx::be::unsafe::load<intx::uint256>((const uint8_t *)value.data());

    runtime_config rc {
        .allow_special_signature = true,
        .abort_on_failure = true,
        .enforce_chain_id = false,
        .allow_non_self_miner = false
    };

    call_(rc, from.value, to, v, data, gas_limit, get_and_increment_nonce(from));
}

void evm_contract::callotherpay(eosio::name payer, eosio::name from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit) {
    assert_unfrozen();
    require_auth(from);
    require_auth(payer);

    eosio::check(_config->get_evm_version_and_maybe_promote() >= 1, "operation not supported for current evm version");

    // Prepare v
    eosio::check(value.size() == sizeof(intx::uint256), "invalid value");
    intx::uint256 v = intx::be::unsafe::load<intx::uint256>((const uint8_t *)value.data());

    runtime_config rc {
        .allow_special_signature = true,
        .abort_on_failure = true,
        .enforce_chain_id = false,
        .allow_non_self_miner = false,
        .gas_payer = payer,
    };

    call_(rc, from.value, to, v, data, gas_limit, get_and_increment_nonce(from));
}

void evm_contract::admincall(const bytes& from, const bytes& to, const bytes& value, const bytes& data, uint64_t gas_limit) {
    assert_unfrozen();
    require_auth(get_self());

    // Prepare v
    eosio::check(value.size() == sizeof(intx::uint256), "invalid value");
    intx::uint256 v = intx::be::unsafe::load<intx::uint256>((const uint8_t *)value.data());
    
    // Prepare s
    eosio::check(from.size() == kAddressLength, err_msg_invalid_addr);
    uint8_t s_buffer[32] = {};
    memcpy(s_buffer + 32 - kAddressLength, from.data(), kAddressLength);
    intx::uint256 s = intx::be::load<intx::uint256>(s_buffer);
    // pad with '1's
    s |= ((~intx::uint256(0)) << (kAddressLength * 8));

    // Prepare nonce
    auto from_addr = to_address(from);
    auto eos_acct = extract_reserved_address(from_addr);
    uint64_t nonce = 0;
    if (eos_acct) {
        nonce = get_and_increment_nonce(eosio::name(*eos_acct));
    }
    else {
        evm_runtime::state state{get_self(), get_self(), true};
        auto account = state.read_account(from_addr);
        check(!!account, err_msg_invalid_addr);
        nonce = account->nonce;
    }

    runtime_config rc {
        .allow_special_signature = true,
        .abort_on_failure = true,
        .enforce_chain_id = false,
        .allow_non_self_miner = false
    };

    call_(rc, s, to, v, data, gas_limit, nonce);
}

void evm_contract::bridgereg(eosio::name receiver, eosio::name handler, const eosio::asset& min_fee) {
    assert_unfrozen();
    require_auth(receiver);
    require_auth(get_self());  // to temporarily prevent registration of unauthorized accounts

    eosio::check(min_fee.symbol == _config->get_token_symbol(), "unexpected symbol");
    eosio::check(min_fee.amount >= 0, "min_fee cannot be negative");

    auto update_row = [&](auto& row) {
        row.account = receiver;
        row.handler = handler;
        row.min_fee = min_fee;
        row.flags   = message_receiver::FORCE_ATOMIC;
    };

    message_receiver_table message_receivers(get_self(), get_self().value);
    auto it = message_receivers.find(receiver.value);

    if(it == message_receivers.end()) {
        message_receivers.emplace(receiver, update_row);
    } else {
        message_receivers.modify(*it, eosio::same_payer, update_row);
    }

    open(receiver);
}

void evm_contract::bridgeunreg(eosio::name receiver) {
    assert_unfrozen();
    require_auth(receiver);

    message_receiver_table message_receivers(get_self(), get_self().value);
    auto it = message_receivers.find(receiver.value);
    eosio::check(it != message_receivers.end(), "receiver not found");
    message_receivers.erase(*it);
}


void evm_contract::assertnonce(eosio::name account, uint64_t next_nonce) { 
    nextnonces nextnonce_table(get_self(), get_self().value);

    auto next_nonce_iter = nextnonce_table.find(account.value);
    if (next_nonce_iter == nextnonce_table.end()) {
        eosio::check(0 == next_nonce, "wrong nonce");
    }
    else {
        eosio::check(next_nonce_iter->next_nonce == next_nonce, "wrong nonce");
    }
}

void evm_contract::setversion(uint64_t version) {
    require_auth(get_self());
    _config->set_evm_version(version);
}

void evm_contract::updtgasparam(eosio::asset ram_price_mb, uint64_t storage_price) {
    require_auth(get_self());
    _config->update_consensus_parameters(ram_price_mb, storage_price);
}

void evm_contract::setgasparam(uint64_t gas_txnewaccount, 
                                uint64_t gas_newaccount, 
                                uint64_t gas_txcreate, 
                                uint64_t gas_codedeposit, 
                                uint64_t gas_sset) {
    require_auth(get_self());
    _config->update_consensus_parameters2(gas_txnewaccount,
                                gas_newaccount,
                                gas_txcreate,
                                gas_codedeposit,
                                gas_sset);
}

void evm_contract::setgasprices(const gas_prices_type& prices) {
    require_auth(get_self());
    eosio::check(prices.overhead_price.has_value() || prices.storage_price.has_value(), "at least one price must be specified");
    eosio::check(!prices.storage_price.has_value() || *prices.storage_price > 0, "zero storage price is not allowed");
    auto current_version = _config->get_evm_version_and_maybe_promote();
    if(current_version >= 3) {
        _config->enqueue_gas_prices(prices);
    } else {
        _config->set_gas_prices(prices);
    }
}

void evm_contract::setgaslimit(uint64_t ingress_gas_limit) {
    _config->set_ingress_gas_limit(ingress_gas_limit);
}

uint64_t evm_contract::get_gas_price(uint64_t evm_version) {
    if( evm_version >= 3) {
        auto gas_prices = _config->get_gas_prices();
        return gas_prices.get_base_price();
    }
    return _config->get_gas_price();
}

statistics evm_contract::get_statistics() const { 
    statistics_singleton statistics_v(get_self(), get_self().value);
    return statistics_v.get_or_create(get_self(), statistics {
        .version = 0,
        .ingress_bridge_fee_income = { .balance = eosio::asset(0, _config->get_ingress_bridge_fee().symbol), .dust = 0 },
        .gas_fee_income = { .balance = eosio::asset(0, _config->get_ingress_bridge_fee().symbol), .dust = 0 },
    });
}

void evm_contract::set_statistics(const statistics &v) {
    statistics_singleton statistics_v(get_self(), get_self().value);
    statistics_v.set(v, get_self());
}

void evm_contract::swapgastoken(eosio::name new_token_contract, eosio::symbol new_symbol, eosio::name swap_dest_account, string swap_memo) {
    require_auth(get_self());
    
    // config table
    eosio::name old_token_contract = _config->get_token_contract();
    eosio::symbol old_symbol = _config->get_token_symbol();
    _config->swapgastoken(new_token_contract, new_symbol);

    // inevm table
    inevm_singleton inevm(get_self(), get_self().value);
    balance_with_dust inevm_row = inevm.get();
    inevm_row.balance.symbol = new_symbol;
    inevm.set(inevm_row, eosio::same_payer);

    // msgreceiver
    message_receiver_table message_receivers(get_self(), get_self().value);
    for (auto it = message_receivers.begin(); it != message_receivers.end(); ++it) {
        message_receivers.modify(*it, eosio::same_payer, [&](message_receiver &row) {
            row.min_fee.symbol = new_symbol;
        });
    }

    // stat
    statistics stat = get_statistics();
    stat.gas_fee_income.balance.symbol = new_symbol;
    stat.ingress_bridge_fee_income.balance.symbol = new_symbol;
    set_statistics(stat);

    // balance table, self row
    migratebal(get_self(), 1);

    if (swap_dest_account != eosio::name()) { 
        // send full balance of old token to swap_dest_account
        token::accounts_table_t self_bal(old_token_contract, get_self().value);
        auto it = self_bal.find(old_symbol.code().raw());
        eosio::check(it != self_bal.end(), "can't get self balance from old token contract");
        token::transfer_action transfer_act(old_token_contract, {{get_self(), "active"_n}});
        transfer_act.send(get_self(), swap_dest_account, it->balance, swap_memo);
    }
}

void evm_contract::migratebal(eosio::name from_name, int limit) {
    int count = 0;
    balances balance_table(get_self(), get_self().value);
    inevm_singleton inevm(get_self(), get_self().value);
    eosio::symbol new_gas_symbol = inevm.get().balance.symbol;
    for (auto it = balance_table.lower_bound(from_name); limit > 0 && it != balance_table.end(); ++it, --limit) {
        if (it->balance.balance.symbol != new_gas_symbol) {
            balance_table.modify(*it, eosio::same_payer, [&](balance &row){
                row.balance.balance.symbol = new_gas_symbol;
            });
            ++count;
        }
    }
    eosio::check(count > 0, "nothing changed");
}

} //evm_runtime
