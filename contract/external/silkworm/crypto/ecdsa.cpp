/*
   Copyright 2020-2021 The Silkworm Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "ecdsa.hpp"

#include <eosio/print.hpp>
#include <eosio/crypto.hpp>

#include <ethash/hash_types.hpp>
#include <ethash/keccak.hpp>

namespace eosio {
   namespace internal_use_do_not_use {
    extern "C" {
      __attribute__((eosio_wasm_import))
      int32_t k1_recover( const char* sig, uint32_t sig_len, const char* dig, uint32_t dig_len, char* pub, uint32_t pub_len);
      }
   }
}


namespace silkworm::ecdsa {

intx::uint256 y_parity_and_chain_id_to_v(bool odd, const std::optional<intx::uint256>& chain_id) noexcept {
    if (chain_id.has_value()) {
        return chain_id.value() * 2 + 35 + odd;
    } else {
        return odd ? 28 : 27;
    }
}

std::optional<YParityAndChainId> v_to_y_parity_and_chain_id(const intx::uint256& v) noexcept {
    YParityAndChainId res{};
    if (v == 27 || v == 28) {
        // pre EIP-155
        res.odd = v == 28;
        res.chain_id = std::nullopt;
    } else if (v < 35) {
        // EIP-155 implies v >= 35
        return std::nullopt;
    } else {
        // https://eips.ethereum.org/EIPS/eip-155
        // Find chain_id and y_parity ∈ {0, 1} such that
        // v = chain_id * 2 + 35 + y_parity
        intx::uint256 w{v - 35};
        res.odd = static_cast<uint64_t>(w) % 2;
        res.chain_id.emplace(w >> 1);  // w / 2
    }
    return res;
}

bool is_valid_signature(const intx::uint256& r, const intx::uint256& s, bool homestead) noexcept {
    if (!r || !s) {
        return false;
    }
    if (r >= kSecp256k1n && s >= kSecp256k1n) {
        return false;
    }
    // https://eips.ethereum.org/EIPS/eip-2
    if (homestead && s > kSecp256k1Halfn) {
        return false;
    }
    return true;
}

std::optional<Bytes> recover(ByteView message, ByteView signature, bool odd_y_parity) noexcept {

    if (message.length() != 32 || signature.length() != 64) {
        return std::nullopt;
    }

    std::vector<uint8_t> vrs(65);
    vrs[0] = odd_y_parity ? 1 : 0;
    vrs[0] += (27 + 4);
    memcpy(&vrs[1], signature.data(), signature.length());

    uint8_t uncompressed_pubkey[65];
    int32_t res = eosio::internal_use_do_not_use::k1_recover((const char*)vrs.data(), vrs.size(), (const char*)message.data(), message.size(), (char *)uncompressed_pubkey, 65);
    if(res) {
        return {};
    }

    return Bytes{uncompressed_pubkey, std::end(uncompressed_pubkey)};
}

std::optional<evmc::address> public_key_to_address(const Bytes& public_key) noexcept {
    if (public_key.length() != 65 || public_key[0] != 4u) {
        return std::nullopt;
    }
    // Ignore first byte of public key
    const auto key_hash{ethash::keccak256(public_key.data() + 1, 64)};
    return evmc::address(*reinterpret_cast<const evmc_address*>(&key_hash.bytes[12]));
}

std::optional<evmc::address> recover_address(ByteView message, ByteView signature, bool odd_y_parity) noexcept {
    const auto recovered_public_key{recover(message, signature, odd_y_parity)};
    return public_key_to_address(recovered_public_key.value_or(Bytes{}));
}

}  // namespace silkworm::ecdsa
