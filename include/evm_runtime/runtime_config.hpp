#pragma once

namespace evm_runtime {

struct runtime_config {
  bool allow_special_signature = false;
  bool abort_on_failure        = false;
  bool enforce_chain_id        = true;
  bool allow_non_self_miner    = true;
};

} //namespace evm_runtime