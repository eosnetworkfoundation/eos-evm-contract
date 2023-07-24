#include "evm_bridge_receiver.hpp"

extern "C" {
__attribute__((eosio_wasm_import))
void set_action_return_value(void*, size_t);
}

struct msgin {
   name       receiver;
   bytes      sender;
   time_point timestamp;
   bytes      value;
   bytes      data;

   EOSLIB_SERIALIZE(msgin, (receiver)(sender)(timestamp)(value)(data));
};

void evm_bridge_receiver::onbridgemsg(name receiver, const bytes& sender, const time_point& timestamp, const bytes& value, const bytes& data) {

   msgin output {
      .receiver  = receiver,
      .sender    = sender,
      .timestamp = timestamp,
      .value     = value,
      .data      = data
   };

   auto output_bin = eosio::pack(output);
   set_action_return_value(output_bin.data(), output_bin.size());
}
