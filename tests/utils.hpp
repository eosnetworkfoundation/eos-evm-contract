#include <filesystem>

#include <fc/log/logger.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/signature.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/fixed_bytes.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/testing/tester.hpp>
#include <fc/variant_object.hpp>
#include <eosio/chain/types.hpp>

#include <silkworm/common/as_range.hpp>
#include <silkworm/common/cast.hpp>
#include <silkworm/common/endian.hpp>
#include <silkworm/common/rlp_err.hpp>
#include <silkworm/common/stopwatch.hpp>
#include <silkworm/common/terminal.hpp>
#include <silkworm/common/test_util.hpp>
#include <silkworm/types/block.hpp>
#include <silkworm/types/transaction.hpp>
#include <silkworm/rlp/encode.hpp>

#include <silkworm/state/state.hpp>
#include <silkworm/consensus/blockchain.hpp>

#include <nlohmann/json.hpp>
#include <ethash/keccak.hpp>

using namespace eosio;
using namespace std;
typedef intx::uint<256> u256;

key256_t to_key256(const uint8_t* ptr, size_t len);
key256_t to_key256(const evmc::address& addr);
key256_t to_key256(const evmc::bytes32& data);
key256_t to_key256(const bytes& data);
bytes to_bytes(const u256& val);
bytes to_bytes(const silkworm::Bytes& b);
bytes to_bytes(const silkworm::ByteView& b);
bytes to_bytes(const evmc::bytes32& val);
bytes to_bytes(const evmc::address& addr);
bytes to_bytes(const key256_t& k);
evmc::address to_address(const bytes& addr);

