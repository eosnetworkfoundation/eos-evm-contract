/*
   Copyright 2020-2022 The Silkworm Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <eosio/eosio.hpp>
#include <evm_runtime/processor.hpp>
#include <silkpre/secp256k1n.hpp>

#include <cassert>

#include <silkworm/chain/intrinsic_gas.hpp>
#include <silkworm/chain/protocol_param.hpp>
#include <silkpre/ecdsa.h>

namespace evm_runtime {

ExecutionProcessor::ExecutionProcessor(const Block& block, consensus::IEngine& consensus_engine, State& state,
                                       const ChainConfig& config)
    : state_{state}, consensus_engine_{consensus_engine}, evm_{block, state_, config} {
    evm_.beneficiary = consensus_engine.get_beneficiary(block.header);
}

ValidationResult ExecutionProcessor::pre_validate_transaction(const Transaction& txn, uint64_t block_number, const ChainConfig& config,
                                          const std::optional<intx::uint256>& base_fee_per_gas) const {
    const evmc_revision rev{config.revision(block_number)};

    if (txn.chain_id.has_value()) {
        if (rev < EVMC_SPURIOUS_DRAGON || txn.chain_id.value() != config.chain_id) {
            return ValidationResult::kWrongChainId;
        }
    }

    if (txn.type == Transaction::Type::kEip2930) {
        if (rev < EVMC_BERLIN) {
            return ValidationResult::kUnsupportedTransactionType;
        }
    } else if (txn.type == Transaction::Type::kEip1559) {
        if (rev < EVMC_LONDON) {
            return ValidationResult::kUnsupportedTransactionType;
        }
    } else if (txn.type != Transaction::Type::kLegacy) {
        return ValidationResult::kUnsupportedTransactionType;
    }

    if (base_fee_per_gas.has_value() && txn.max_fee_per_gas < base_fee_per_gas.value()) {
        return ValidationResult::kMaxFeeLessThanBase;
    }

    // https://github.com/ethereum/EIPs/pull/3594
    if (txn.max_priority_fee_per_gas > txn.max_fee_per_gas) {
        return ValidationResult::kMaxPriorityFeeGreaterThanMax;
    }

    /* Should the sender already be present it means the validation of signature already occurred */
    if (!txn.from.has_value()) {
        if (!silkpre::is_valid_signature(txn.r, txn.s, rev >= EVMC_HOMESTEAD)) {
            return ValidationResult::kInvalidSignature;
        }
    }

    const intx::uint128 g0{intrinsic_gas(txn, rev >= EVMC_HOMESTEAD, rev >= EVMC_ISTANBUL)};
    if (txn.gas_limit < g0) {
        return ValidationResult::kIntrinsicGas;
    }

    // EIP-2681: Limit account nonce to 2^64-1
    if (txn.nonce >= UINT64_MAX) {
        return ValidationResult::kNonceTooHigh;
    }

    return ValidationResult::kOk;
}


ValidationResult ExecutionProcessor::validate_transaction(const Transaction& txn) const noexcept {
    auto r = pre_validate_transaction(txn, evm_.block().header.number, evm_.config(), evm_.block().header.base_fee_per_gas);
    if(r != ValidationResult::kOk) {
        eosio::print("ERR:", uint64_t(r));
        eosio::check(false, "pre_validate_transaction error");
    }

    if (!txn.from.has_value()) {
        eosio::print("txn.from.has_value");
        return ValidationResult::kMissingSender;
    }

    if (state_.get_code_hash(*txn.from) != kEmptyHash) {
        eosio::print("state_.get_code_hash");
        return ValidationResult::kSenderNoEOA;  // EIP-3607
    }

    const uint64_t nonce{state_.get_nonce(*txn.from)};
    if (nonce != txn.nonce) {
        eosio::print("nonce");
        return ValidationResult::kWrongNonce;
    }

    // https://github.com/ethereum/EIPs/pull/3594
    const intx::uint512 max_gas_cost{intx::umul(intx::uint256{txn.gas_limit}, txn.max_fee_per_gas)};
    // See YP, Eq (57) in Section 6.2 "Execution"
    const intx::uint512 v0{max_gas_cost + txn.value};
    if (state_.get_balance(*txn.from) < v0) {
        eosio::print("state_.get_balance");
        return ValidationResult::kInsufficientFunds;
    }

    if (available_gas() < txn.gas_limit) {
        // Corresponds to the final condition of Eq (58) in Yellow Paper Section 6.2 "Execution".
        // The sum of the transaction’s gas limit and the gas utilized in this block prior
        // must be no greater than the block’s gas limit.
        eosio::print("available_gas");
        return ValidationResult::kBlockGasLimitExceeded;
    }

    return ValidationResult::kOk;
}

void ExecutionProcessor::execute_transaction(const Transaction& txn, Receipt& receipt) noexcept {
    auto res = validate_transaction(txn);
    if(res != ValidationResult::kOk) {
        std::string msg = "validate_transaction error: ";
        msg += std::to_string((int)res);
        eosio::check(false, msg.c_str());
    }
    //eosio::check(validate_transaction(txn) == ValidationResult::kOk, "validate_transaction error");

    // Optimization: since receipt.logs might have some capacity, let's reuse it.
    std::swap(receipt.logs, state_.logs());

    state_.clear_journal_and_substate();

    eosio::check(txn.from.has_value(), "no from");
    state_.access_account(*txn.from);

    const intx::uint256 base_fee_per_gas{evm_.block().header.base_fee_per_gas.value_or(0)};
    const intx::uint256 effective_gas_price{txn.effective_gas_price(base_fee_per_gas)};
    state_.subtract_from_balance(*txn.from, txn.gas_limit * effective_gas_price);

    if (txn.to.has_value()) {
        state_.access_account(*txn.to);
        // EVM itself increments the nonce for contract creation
        state_.set_nonce(*txn.from, txn.nonce + 1);
    }

    for (const AccessListEntry& ae : txn.access_list) {
        state_.access_account(ae.account);
        for (const evmc::bytes32& key : ae.storage_keys) {
            state_.access_storage(ae.account, key);
        }
    }

    const evmc_revision rev{evm_.revision()};

    const intx::uint128 g0{intrinsic_gas(txn, rev >= EVMC_HOMESTEAD, rev >= EVMC_ISTANBUL)};
    eosio::check(g0 <= UINT64_MAX, "g0 <= UINT64_MAX");  // true due to the precondition (transaction must be valid)

    const CallResult vm_res{evm_.execute(txn, txn.gas_limit - static_cast<uint64_t>(g0))};

    const uint64_t gas_used{txn.gas_limit - refund_gas(txn, vm_res.gas_left, vm_res.gas_refund)};

    // award the fee recipient
    const intx::uint256 priority_fee_per_gas{txn.priority_fee_per_gas(base_fee_per_gas)};
    state_.add_to_balance(evm_.beneficiary, priority_fee_per_gas * gas_used);

    state_.destruct_suicides();
    if (rev >= EVMC_SPURIOUS_DRAGON) {
        state_.destruct_touched_dead();
    }

    state_.finalize_transaction();

    cumulative_gas_used_ += gas_used;

    receipt.type = txn.type;
    receipt.success = vm_res.status == EVMC_SUCCESS;
    receipt.cumulative_gas_used = cumulative_gas_used_;
    std::swap(receipt.logs, state_.logs());

    consensus_engine_.finalize(state_, evm_.block(), evm_.revision());
    state_.write_to_db(evm_.block().header.number);
}

uint64_t ExecutionProcessor::available_gas() const noexcept {
    return evm_.block().header.gas_limit - cumulative_gas_used_;
}

uint64_t ExecutionProcessor::refund_gas(const Transaction& txn, uint64_t gas_left, uint64_t gas_refund) noexcept {
    const evmc_revision rev{evm_.revision()};

    const uint64_t max_refund_quotient{rev >= EVMC_LONDON ? param::kMaxRefundQuotientLondon
                                                          : param::kMaxRefundQuotientFrontier};
    const uint64_t max_refund{(txn.gas_limit - gas_left) / max_refund_quotient};
    uint64_t refund = std::min(gas_refund, max_refund);
    gas_left += refund;

    const intx::uint256 base_fee_per_gas{evm_.block().header.base_fee_per_gas.value_or(0)};
    const intx::uint256 effective_gas_price{txn.effective_gas_price(base_fee_per_gas)};
    state_.add_to_balance(*txn.from, gas_left * effective_gas_price);

    return gas_left;
}

}  // namespace evm_runtime
