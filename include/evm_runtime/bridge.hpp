#pragma once

#include <evm_runtime/types.hpp>
#include <silkworm/core/common/base.hpp>

namespace evm_runtime { namespace bridge {

using namespace std;
using namespace eosio;
using namespace silkworm;
struct message_v0 {
    static constexpr uint32_t id = 0xf781185b; //sha3('bridgeMsgV0(string,bool,bytes)')[:4]

    string account;
    bool   force_atomic; //currently only atomic is supported
    bytes  data;

    name get_account_as_name() const {
        if(!account_.has_value()) {
            account_ = name{account};
            eosio::check(account_.value().to_string() == account, "invalid account name");
        }
        return *account_;
    }

    mutable std::optional<name> account_;
};

using message = std::variant<message_v0>;

message_v0 decode_message_v0(eosio::datastream<const uint8_t*>& ds) {
    // offset_p1 (32) + p2_value (32) + offset_p3 (32)
    // p1_len    (32) + p1_data   ((p1_len+31)/32*32)
    // p3_len    (32) + p3_data   ((p2_len+31)/32*32)
    uint256  offset_p1, value_p2, offset_p3;

    ds >> offset_p1;
    eosio::check(offset_p1 == 0x60, "invalid p1 offset");
    ds >> value_p2;
    eosio::check(value_p2 <= 1, "invalid p2 value");
    ds >> offset_p3;
    eosio::check(offset_p3 == 0xA0, "invalid p3 offset");

    message_v0 res;
    res.force_atomic = value_p2 ? true : false;

    auto get_length=[&]() -> uint32_t {
        uint256 len;
        ds >> len;
        eosio::check(len < std::numeric_limits<uint32_t>::max(), "invalid length");
        return static_cast<uint32_t>(len);
    };

    uint32_t p1_len = get_length();
    auto p1_len_32 = (p1_len+31)/32*32;
    res.account.resize(p1_len_32);
    ds.read(res.account.data(), p1_len_32);
    res.account.resize(p1_len);

    uint32_t p3_len = get_length();
    auto p3_len_32 = (p3_len+31)/32*32;
    res.data.resize(p3_len_32);
    ds.read(res.data.data(), p3_len_32);
    res.data.resize(p3_len);

    return res;
}

std::optional<message> decode_message(ByteView bv) {
    // method_id (4)
    eosio::datastream<const uint8_t*> ds(bv.data(), bv.size());
    uint32_t method_id;
    ds >> method_id;

    if(method_id == __builtin_bswap32(message_v0::id)) return decode_message_v0(ds);
    return {};
}

} //namespace bridge
} //namespace evm_runtime
