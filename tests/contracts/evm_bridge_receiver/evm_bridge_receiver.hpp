#include <vector>
#include <variant>
#include <eosio/eosio.hpp>

using namespace eosio;

typedef std::vector<char> bytes;

struct bridge_message_v0 {
   name       receiver;
   bytes      sender;
   time_point timestamp;
   bytes      value;
   bytes      data;

   EOSLIB_SERIALIZE(bridge_message_v0, (receiver)(sender)(timestamp)(value)(data));
};

using bridge_message = std::variant<bridge_message_v0>;

CONTRACT evm_bridge_receiver : public contract {
   public:
      using contract::contract;
      [[eosio::action]] void onbridgemsg(const bridge_message& msg);
};