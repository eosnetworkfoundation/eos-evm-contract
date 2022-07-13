#pragma once

namespace evm_runtime {

struct print_tracer : EvmTracer {

    void on_execution_start(evmc_revision rev, const evmc_message& msg, evmone::bytes_view code) override {
        eosio::print("TRACE: start\n");
    };

    void on_instruction_start(uint32_t pc, const evmone::ExecutionState& state,
                            const IntraBlockState& intra_block_state) override {
        eosio::print("TRACE[inst] ", uint64_t(pc), ": ", uint64_t(state.gas_left), "\n");

    };

    void on_execution_end(const evmc_result& result, const IntraBlockState& intra_block_state) override {
        eosio::print("TRACE: end\n");
    }

};

} //namespace evm_runtime