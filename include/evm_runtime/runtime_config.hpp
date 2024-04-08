#pragma once
#include <variant>
namespace evm_runtime {
struct runtime_config {
  bool allow_special_signature = false;
  bool abort_on_failure        = false;
  bool enforce_chain_id        = true;
  bool allow_non_self_miner    = true;
};

struct gas_parameter_type {
    uint64_t gas_txnewaccount = 0;
    uint64_t gas_newaccount = 25000;
    uint64_t gas_txcreate = 32000;
    uint64_t gas_codedeposit = 200;
    uint64_t gas_sset = 20000;
};
struct consensus_parameter_data_v0 {
    gas_parameter_type gas_parameter;
};
using consensus_parameter_data_type = std::variant<consensus_parameter_data_v0>;

} //namespace evm_runtime