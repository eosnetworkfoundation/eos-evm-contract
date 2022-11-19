#pragma once

#include <eosio/ship_protocol.hpp>

#include <silkworm/common/util.hpp>
#include <silkworm/common/log.hpp>

namespace utils {

    inline std::string to_string(const eosio::checksum256& cs) {
        auto bytes = cs.extract_as_byte_array();
        return silkworm::to_hex(bytes, false);
    }

    inline eosio::checksum256 checksum256_from_string(std::string_view s) {
        std::optional<silkworm::Bytes> bytes = silkworm::from_hex(s);
        if ( bytes ) {
            std::array<uint8_t, 32> data{};
            if ( data.size() == bytes->size() ) {
                std::memcpy(data.data(), bytes->data(), bytes->size());
                return eosio::checksum256{data};
            }
        }

        std::string err = "Unable to convert to checksum256: ";
        err += s;
        throw std::runtime_error(err);
    }

    inline uint32_t endian_reverse_u32( uint32_t x ) {
        return (((x >> 0x18) & 0xFF)        )
               | (((x >> 0x10) & 0xFF) << 0x08)
               | (((x >> 0x08) & 0xFF) << 0x10)
               | (((x        ) & 0xFF) << 0x18)
            ;
    }

    inline uint32_t to_block_num(const evmc::bytes32& b) {
        return endian_reverse_u32(*reinterpret_cast<const uint32_t*>(b.bytes));
    }

    inline uint32_t to_block_num(const eosio::checksum256& cs) {
        return endian_reverse_u32(*reinterpret_cast<uint32_t*>(cs.extract_as_byte_array().data()));
    }

} // namespace utils
