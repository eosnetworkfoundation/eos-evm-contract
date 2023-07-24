#include <vector>
#include <eosio/eosio.hpp>

using namespace eosio;

typedef std::vector<char> bytes;
CONTRACT evm_bridge_receiver : public contract {
   public:
      using contract::contract;
      [[eosio::action]] void onbridgemsg(name receiver, const bytes& sender, const time_point& timestamp, const bytes& value, const bytes& data);
};