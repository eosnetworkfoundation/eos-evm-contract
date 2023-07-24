#pragma once

#include <evm_runtime/types.hpp>

namespace evm_runtime { namespace bridge {

struct emit {
    static constexpr uint32_t id = 0x7e95a247; //sha3('emit_(string,bytes)')[:4]

    string account;
    bytes  data;

    name get_account_as_name() const {
        if(!account_.has_value()) {
            account_ = name{account};
            eosio::check(account_.value().to_string() == account, "invalid account name");
        }
        return *account_;
    }

    eosio::time_point   timestamp;
    evmc_message        message;
    mutable std::optional<name> account_;
};

using call = std::variant<emit>;

std::optional<call> decode_emit_message(eosio::datastream<const uint8_t*>& ds) {
    // offset_p1 (32) + offset_p2 (32)
    // p1_len    (32) + p1_data   ((p1_len+31)/32*32)
    // p2_len    (32) + p1_data   ((p2_len+31)/32*32)
    uint256  offset_p1, offset_p2;
    uint32_t p1_len, p2_len;

    ds >> offset_p1;
    eosio::check(offset_p1 == 0x40, "invalid p1 offset");
    ds >> offset_p2;
    eosio::check(offset_p2 == 0x80, "invalid p2 offset");

    emit res;

    auto get_length=[&]() -> uint32_t {
        uint256 len;
        ds >> len;
        eosio::check(len < std::numeric_limits<uint32_t>::max(), "invalid length");
        return static_cast<uint32_t>(len);
    };

    p1_len = get_length();
    auto p1_len_32 = (p1_len+31)/32*32;
    res.account.resize(p1_len_32);
    ds.read(res.account.data(), p1_len_32);
    res.account.resize(p1_len);

    p2_len = get_length();
    auto p2_len_32 = (p2_len+31)/32*32;
    res.data.resize(p2_len_32);
    ds.read(res.data.data(), p2_len_32);
    res.data.resize(p2_len);

    return res;
}

std::optional<call> decode_call_message(ByteView bv) {
    // method_id (4)
    eosio::datastream<const uint8_t*> ds(bv.data(), bv.size());
    uint32_t method_id;
    ds >> method_id;

    if(method_id == __builtin_bswap32(emit::id)) return decode_emit_message(ds);
    return {};
}

} //namespace bridge
} //namespace evm_runtime
