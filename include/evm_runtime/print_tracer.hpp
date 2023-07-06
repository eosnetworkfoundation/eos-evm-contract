#pragma once
#include <evmc/instructions.h>
namespace evm_runtime {

struct print_tracer : EvmTracer {

    const char* const* opcode_names_ = nullptr;

    void on_execution_start(evmc_revision rev, const evmc_message& msg, evmone::bytes_view code) noexcept override {
        eosio::print("TRACE: start\n");
        if (opcode_names_ == nullptr) {
            opcode_names_ = evmc_get_instruction_names_table(rev);
        }
    }

    std::string get_opcode_name(const char* const* names, std::uint8_t opcode) {
        const auto name = names[opcode];
        return (name != nullptr) ?name : "opcode 0x" + evmc::hex(opcode) + " not defined";
    }

    void on_instruction_start(uint32_t pc, const intx::uint256* stack_top, int stack_height,
                                      const evmone::ExecutionState& state,
                                      const IntraBlockState& intra_block_state) noexcept override {
        const auto opcode = state.code[pc];
        auto opcode_name = get_opcode_name(opcode_names_, opcode);
        eosio::print(opcode_name.c_str(), "\n");
    }

    void on_execution_end(const evmc_result& result, const IntraBlockState& intra_block_state) noexcept override {
        eosio::print("TRACE: end\n");
    }

    void on_creation_completed(const evmc_result& result, const IntraBlockState& intra_block_state) noexcept override {

    }

    void on_precompiled_run(const evmc_result& result, int64_t gas,
                                    const IntraBlockState& intra_block_state) noexcept override {

    }

    void on_reward_granted(const CallResult& result, const IntraBlockState& intra_block_state) noexcept override {

    }

};
} //namespace evm_runtime