/*
   Copyright 2020-2021 The Silkworm Authors

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

#ifndef SILKWORM_EXECUTION_PROCESSOR_HPP_
#define SILKWORM_EXECUTION_PROCESSOR_HPP_

#include <cstdint>
#include <vector>

#include <silkworm/consensus/engine.hpp>
#include <silkworm/execution/evm.hpp>
#include <silkworm/state/state.hpp>
#include <silkworm/types/block.hpp>
#include <silkworm/types/receipt.hpp>
#include <silkworm/types/transaction.hpp>

namespace evm_runtime {

using namespace silkworm;

class ExecutionProcessor {
  public:
    ExecutionProcessor(const ExecutionProcessor&) = delete;
    ExecutionProcessor& operator=(const ExecutionProcessor&) = delete;

    ExecutionProcessor(const Block& block, consensus::IEngine& engine, State& state, const ChainConfig& config);

    // Preconditions:
    // 1) consensus' pre_validate_transaction(txn) must return kOk
    // 2) txn.from must be recovered, otherwise kMissingSender will be returned
    ValidationResult validate_transaction(const Transaction& txn) const noexcept;
    ValidationResult pre_validate_transaction(const Transaction& txn, uint64_t block_number, const ChainConfig& config,
                                          const std::optional<intx::uint256>& base_fee_per_gas) const;

    // Execute a transaction, but do not write to the DB yet.
    // Precondition: transaction must be valid.
    void execute_transaction(const Transaction& txn, Receipt& receipt) noexcept;

    uint64_t cumulative_gas_used() const noexcept { return cumulative_gas_used_; }

    EVM& evm() noexcept { return evm_; }
    const EVM& evm() const noexcept { return evm_; }

  private:
    uint64_t available_gas() const noexcept;
    uint64_t refund_gas(const Transaction& txn, uint64_t gas_left) noexcept;

    uint64_t cumulative_gas_used_{0};
    IntraBlockState state_;
    consensus::IEngine& consensus_engine_;
    EVM evm_;
};

}  // namespace evm_runtime

#endif  // SILKWORM_EXECUTION_PROCESSOR_HPP_
