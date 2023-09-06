#include "evm_bridge_receiver.hpp"

extern "C" {
__attribute__((eosio_wasm_import))
void set_action_return_value(void*, size_t);
}

void evm_bridge_receiver::onbridgemsg(const bridge_message& msg) {
   auto output_bin = eosio::pack(msg);
   set_action_return_value(output_bin.data(), output_bin.size());
}
