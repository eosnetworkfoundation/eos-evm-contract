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
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>

#include "precompiled.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

#include <ethash/keccak.hpp>

#include <silkworm/chain/protocol_param.hpp>
#include <silkworm/common/endian.hpp>
#include <silkworm/common/util.hpp>
#include <silkworm/crypto/ecdsa.hpp>

namespace eosio {
   namespace internal_use_do_not_use {
    extern "C" {
      __attribute__((eosio_wasm_import)) 
      int32_t alt_bn128_add( const char* op1, uint32_t op1_len, const char* op2, uint32_t op2_len, char* result, uint32_t result_len);

      __attribute__((eosio_wasm_import)) 
      int32_t alt_bn128_mul( const char* g1, uint32_t g1_len, const char* scalar, uint32_t scalar_len, char* result, uint32_t result_len);

      __attribute__((eosio_wasm_import)) 
      int32_t alt_bn128_pair( const char* pairs, uint32_t pairs_len);

      __attribute__((eosio_wasm_import)) 
      int32_t mod_exp( const char* base, uint32_t base_len, const char* exp, uint32_t exp_len, const char* mod, uint32_t mod_len, char* result, uint32_t result_len);

      __attribute__((eosio_wasm_import))
      int32_t blake2_f( uint32_t rounds, const char* state, uint32_t state_len, const char* msg, uint32_t msg_len, 
                  const char* t0_offset, uint32_t t0_len, const char* t1_offset, uint32_t t1_len, int32_t final, char* result, uint32_t result_len);

      __attribute__((eosio_wasm_import))
      void sha3( const char* data, uint32_t data_len, char* hash, uint32_t hash_len, int32_t keccak );
   }
  }
}

namespace silkworm::precompiled {

uint64_t ecrec_gas(ByteView, evmc_revision) noexcept { return 3'000; }

std::optional<Bytes> ecrec_run(ByteView input) noexcept {

    static constexpr size_t kInputLen{128};
    Bytes d{input};
    if (d.length() < kInputLen) {
        d.resize(kInputLen, '\0');
    }

    const auto v{intx::be::unsafe::load<intx::uint256>(&d[32])};
    const auto r{intx::be::unsafe::load<intx::uint256>(&d[64])};
    const auto s{intx::be::unsafe::load<intx::uint256>(&d[96])};

    const bool homestead{false};  // See EIP-2
    if (!ecdsa::is_valid_signature(r, s, homestead)) {
        return Bytes{};
    }

    const std::optional<ecdsa::YParityAndChainId> parity_and_id{ecdsa::v_to_y_parity_and_chain_id(v)};
    if (parity_and_id == std::nullopt || parity_and_id->chain_id != std::nullopt) {
        return Bytes{};
    }

    /*
     * Note ! We could replace this block of code with the one commented below (kept for reference) which directly
     * returns address. This could bring the advantage to have validation of public key to address (first byte == 4) in
     * a single place (crypto/ecdsa.cpp) but also incurs in an extra memcpy due to keccak -> evmc::address -> bytes[32].
     * Here instead we copy only keccak -> bytes32
     */

    // TODO (yperbasis) : check why is necessary to return a Byte[32] while actually the first 12 bytes are zeroes

    const std::optional<Bytes> key{ecdsa::recover(d.substr(0, 32), d.substr(64, 64), parity_and_id->odd)};
    if (key.has_value() && key->at(0) == 4u) {
        // Ignore the first byte of the public key
        const ethash::hash256 hash{ethash::keccak256(key->data() + 1, key->length() - 1)};
        Bytes out(32, '\0');
        std::memcpy(&out[12], &hash.bytes[12], 32 - 12);
        return out;
    }
    return Bytes{};

}

uint64_t sha256_gas(ByteView input, evmc_revision) noexcept { return 60 + 12 * ((input.length() + 31) / 32); }

std::optional<Bytes> sha256_run(ByteView input) noexcept {
    auto res = eosio::sha256((const char*)input.data(), input.size()).extract_as_byte_array();
    return Bytes{res.begin(), res.end()};
}

uint64_t rip160_gas(ByteView input, evmc_revision) noexcept { return 600 + 120 * ((input.length() + 31) / 32); }

std::optional<Bytes> rip160_run(ByteView input) noexcept {
    Bytes out(32,'\0');
    auto res = eosio::ripemd160((const char*)input.data(), input.size()).extract_as_byte_array();
    memcpy(out.data()+12, res.data(), res.size());
    return out;
}

uint64_t id_gas(ByteView input, evmc_revision) noexcept { return 15 + 3 * ((input.length() + 31) / 32); }

std::optional<Bytes> id_run(ByteView input) noexcept { return Bytes{input}; }

static intx::uint256 mult_complexity_eip198(const intx::uint256& x) noexcept {
    const intx::uint256 x_squared{x * x};
    if (x <= 64) {
        return x_squared;
    } else if (x <= 1024) {
        return (x_squared >> 2) + 96 * x - 3072;
    } else {
        return (x_squared >> 4) + 480 * x - 199680;
    }
}

static intx::uint256 mult_complexity_eip2565(const intx::uint256& max_length) noexcept {
    const intx::uint256 words{(max_length + 7) >> 3};  // ⌈max_length/8⌉
    return words * words;
}

uint64_t expmod_gas(ByteView input, evmc_revision rev) noexcept {
    const uint64_t min_gas{rev < EVMC_BERLIN ? 0 : 200u};

    Bytes buffer;
    input = right_pad(input, 3 * 32, buffer);

    intx::uint256 base_len256{intx::be::unsafe::load<intx::uint256>(&input[0])};
    intx::uint256 exp_len256{intx::be::unsafe::load<intx::uint256>(&input[32])};
    intx::uint256 mod_len256{intx::be::unsafe::load<intx::uint256>(&input[64])};

    if (base_len256 == 0 && mod_len256 == 0) {
        return min_gas;
    }

    if (intx::count_significant_words(base_len256) > 1 || intx::count_significant_words(exp_len256) > 1 ||
        intx::count_significant_words(mod_len256) > 1) {
        return UINT64_MAX;
    }

    uint64_t base_len64{static_cast<uint64_t>(base_len256)};
    uint64_t exp_len64{static_cast<uint64_t>(exp_len256)};

    input.remove_prefix(3 * 32);

    intx::uint256 exp_head{0};  // first 32 bytes of the exponent
    if (input.length() > base_len64) {
        ByteView exp_input{right_pad(input.substr(base_len64), 32, buffer)};
        if (exp_len64 < 32) {
            exp_input = exp_input.substr(0, exp_len64);
            exp_input = left_pad(exp_input, 32, buffer);
        }
        exp_head = intx::be::unsafe::load<intx::uint256>(exp_input.data());
    }
    unsigned bit_len{256 - clz(exp_head)};

    intx::uint256 adjusted_exponent_len{0};
    if (exp_len256 > 32) {
        adjusted_exponent_len = 8 * (exp_len256 - 32);
    }
    if (bit_len > 1) {
        adjusted_exponent_len += bit_len - 1;
    }

    if (adjusted_exponent_len < 1) {
        adjusted_exponent_len = 1;
    }

    const intx::uint256 max_length{std::max(mod_len256, base_len256)};

    intx::uint256 gas;
    if (rev < EVMC_BERLIN) {
        gas = mult_complexity_eip198(max_length) * adjusted_exponent_len / param::kGQuadDivisorByzantium;
    } else {
        gas = mult_complexity_eip2565(max_length) * adjusted_exponent_len / param::kGQuadDivisorBerlin;
    }

    if (intx::count_significant_words(gas) > 1) {
        return UINT64_MAX;
    } else {
        return std::max(min_gas, static_cast<uint64_t>(gas));
    }
}

std::optional<Bytes> expmod_run(ByteView input) noexcept {
    Bytes buffer;
    input = right_pad(input, 3 * 32, buffer);

    //TODO: should we assert here if length is using more than 4 bytes?

    uint32_t base_len{endian::load_big_u32(&input[28])};
    input.remove_prefix(32);

    uint32_t exponent_len{endian::load_big_u32(&input[28])};
    input.remove_prefix(32);

    uint32_t modulus_len{endian::load_big_u32(&input[28])};
    input.remove_prefix(32);

    if (modulus_len == 0) {
        return Bytes{};
    }

    input = right_pad(input, base_len + exponent_len + modulus_len, buffer);

    auto base_data     = (const char*)input.data();
    auto exponent_data = base_data + base_len;
    auto modulus_data  = exponent_data + exponent_len;

    Bytes result(modulus_len, '\0');
    auto err = eosio::internal_use_do_not_use::mod_exp(base_data, base_len, exponent_data, exponent_len, modulus_data, modulus_len, (char *)result.data(), result.size());
    if(err < 0) {
        return Bytes{};
    }

    return result;
}

uint64_t bn_add_gas(ByteView, evmc_revision rev) noexcept { return rev >= EVMC_ISTANBUL ? 150 : 500; }

std::optional<Bytes> bn_add_run(ByteView input) noexcept {
    Bytes buffer;
    input = right_pad(input, 128, buffer);

    auto op1_data = (const char*)input.data();
    auto op2_data = op1_data + 64;

    Bytes result(64, '\0');
    auto err = eosio::internal_use_do_not_use::alt_bn128_add( op1_data, 64, op2_data, 64, (char *)result.data(), result.size());
    if(err < 0) {
        return std::nullopt;
    }

    return result;
}

uint64_t bn_mul_gas(ByteView, evmc_revision rev) noexcept { return rev >= EVMC_ISTANBUL ? 6'000 : 40'000; }

std::optional<Bytes> bn_mul_run(ByteView input) noexcept {
    Bytes buffer;
    input = right_pad(input, 96, buffer);

    auto point_data  = (const char*)input.data();
    auto scalar_data = point_data + 64;

    Bytes result(64, '\0');
    auto err = eosio::internal_use_do_not_use::alt_bn128_mul( point_data, 64, scalar_data, 32, (char *)result.data(), result.size());
    if(err < 0) {
        return std::nullopt;
    }

    return result;
}

static constexpr size_t kSnarkvStride{192};

uint64_t snarkv_gas(ByteView input, evmc_revision rev) noexcept {
    uint64_t k{input.length() / kSnarkvStride};
    return rev >= EVMC_ISTANBUL ? 34'000 * k + 45'000 : 80'000 * k + 100'000;
}

std::optional<Bytes> snarkv_run(ByteView input) noexcept {
    if (input.size() % kSnarkvStride != 0) {
        return std::nullopt;
    }
    size_t k{input.size() / kSnarkvStride};

    auto err = eosio::internal_use_do_not_use::alt_bn128_pair( (const char*)input.data(), input.size());
    if(err < 0) {
        return std::nullopt;
    }

    Bytes out(32, '\0');
    if (err == 0) {
        out[31] = 1;
    }
    return out;
}

uint64_t blake2_f_gas(ByteView input, evmc_revision) noexcept {
    if (input.length() < 4) {
        // blake2_f_run will fail anyway
        return 0;
    }
    return endian::load_big_u32(input.data());
}

std::optional<Bytes> blake2_f_run(ByteView input) noexcept {
    if (input.size() != 213) {
        return std::nullopt;
    }
    uint8_t f{input[212]};
    if (f != 0 && f != 1) {
        return std::nullopt;
    }

    auto rounds    = endian::load_big_u32(input.data());
    auto state     = (const char *)input.data() + 4;
    auto message   = state + 64;
    auto t0_offset = message + 128;
    auto t1_offset = t0_offset + 8;

    Bytes result(64, '\0');
    auto err = eosio::internal_use_do_not_use::blake2_f(rounds, state, 64, message, 128, t0_offset, 8, t1_offset, 8, (bool)f, (char *)result.data(), result.size());
    if(err < 0) {
        return std::nullopt;
    }
    return result;
}

}  // namespace silkworm::precompiled
