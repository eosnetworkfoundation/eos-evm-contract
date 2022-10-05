#pragma once

namespace evm_runtime {

struct engine : silkworm::consensus::IEngine {
    
    ValidationResult pre_validate_block_body(const Block& block, const BlockState& state) override {
        return ValidationResult::kOk;
    }

    ValidationResult validate_ommers(const Block& block, const BlockState& state) override {
        return ValidationResult::kOk;
    }

    ValidationResult validate_block_header(const BlockHeader& header, const BlockState& state,
                                                   bool with_future_timestamp_check) override {
        return ValidationResult::kOk;
    }

    ValidationResult validate_seal(const BlockHeader& header) override {
        return ValidationResult::kOk;
    }

    void finalize(IntraBlockState& state, const Block& block, evmc_revision revision) override {

    }

    evmc::address get_beneficiary(const BlockHeader& header) override {
        return header.beneficiary;
    }
};

} //namespace evm_runtime