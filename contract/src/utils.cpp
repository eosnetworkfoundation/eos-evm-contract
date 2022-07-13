#include <eosio/eosio.hpp>
#include <eosio/fixed_bytes.hpp>
#include <evm_runtime/types.hpp>

namespace evm_runtime {

using namespace eosio;

checksum256 make_key(const uint8_t* ptr, size_t len) {
    uint8_t buffer[32]={0};
    check(len <= sizeof(buffer), "invalida size");
    memcpy(buffer, ptr, len);
    return checksum256(buffer);
}

checksum256 make_key(bytes data){
    return make_key((const uint8_t*)data.data(), data.size());
}

checksum256 make_key(const evmc::address& addr){
    return make_key(addr.bytes, sizeof(addr.bytes));
}

checksum256 make_key(const evmc::bytes32& data){
    return make_key(data.bytes, sizeof(data.bytes));
}

bytes to_bytes(const uint256& val) {
    uint8_t tmp[32];
    intx::be::store(tmp, val);
    return bytes{tmp, std::end(tmp)};
}

bytes to_bytes(const evmc::bytes32& val) {
    return bytes{val.bytes, std::end(val.bytes)};
}

bytes to_bytes(const evmc::address& addr) {
    return bytes{addr.bytes, std::end(addr.bytes)};
}

evmc::address to_address(const bytes& addr) {
    evmc::address res;
    memcpy(res.bytes, addr.data(), 20);
    return res;
}

evmc::bytes32 to_bytes32(const bytes& data) {
    evmc::bytes32 res;
    eosio::check(data.size() == 32, "wrong length");
    memcpy(res.bytes, data.data(), data.size());
    return res;
}

uint256 to_uint256(const bytes& value) {
    uint8_t tmp[32]{0};
    eosio::check(value.size() <= 32, "wrong length");
    size_t l = 32-value.size();
    memcpy(tmp+l, value.data(), value.size());
    return intx::be::load<uint256>(tmp);
}

}  // namespace silkworm
