#include <filesystem>

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain/authorization_manager.hpp>
#include <cstdlib>
#include <iostream>
#include <fc/log/logger.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/signature.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/fixed_bytes.hpp>

#include "eosio.system_tester.hpp"

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

using namespace eosio_system;
using namespace eosio;
using namespace std;
using namespace fc::crypto;
using namespace silkworm;
using namespace silkworm::rlp;

namespace fs = std::filesystem;
typedef intx::uint<256> u256;

enum class Status { kPassed, kFailed, kSkipped };
struct [[nodiscard]] RunResults {
    size_t passed{0};
    size_t failed{0};
    size_t skipped{0};

    constexpr RunResults() = default;

    constexpr RunResults(Status status) {
        switch (status) {
            case Status::kPassed:
                passed = 1;
                return;
            case Status::kFailed:
                failed = 1;
                return;
            case Status::kSkipped:
                skipped = 1;
                return;
        }
    }

    RunResults& operator+=(const RunResults& rhs) {
        passed += rhs.passed;
        failed += rhs.failed;
        skipped += rhs.skipped;
        return *this;
    }
};

struct evm_runtime_tester;
using RunnerFunc = RunResults (evm_runtime_tester::*)(const std::string&, const nlohmann::json&);
static constexpr size_t kColumnWidth{80};

static const fs::path kDifficultyDir{"DifficultyTests"};
static const fs::path kBlockchainDir{"BlockchainTests/GeneralStateTests"};
static const fs::path kTransactionDir{"TransactionTests"};

static const std::vector<fs::path> kSlowTests{
    kBlockchainDir / "stTimeConsuming",
    kBlockchainDir / "VMTests" / "vmPerformance",
};


const static account_name ME = account_name("evm");
const static symbol core_symbol = symbol{CORE_SYM};
const static name system_account_name = eosio::chain::config::system_account_name;
struct currency_stats {
   asset    supply;
   asset    max_supply;
   name     issuer;
};
FC_REFLECT(currency_stats, (supply)(max_supply)(issuer));

//contract table objects helper
const table_id_object& find_or_create_table( chainbase::database& db, name code, name scope, name table, const account_name& payer ) {

   const auto* existing_tid =  db.find<table_id_object, by_code_scope_table>(
      boost::make_tuple(code, scope, table)
   );

   if (existing_tid != nullptr) {
      return *existing_tid;
   }

   return db.create<table_id_object>([&](table_id_object &t_id){
      t_id.code = code;
      t_id.scope = scope;
      t_id.table = table;
      t_id.payer = payer;
   });
}

template <typename T, typename Object>
static std::optional<Object> get_by_primary_key(chainbase::database& db, const name& scope, const T& o) {
   const auto& tab = find_or_create_table(
      db, "evm"_n, scope, Object::table_name(), "evm"_n
   );

   const auto* kv_obj = db.find<key_value_object, by_scope_primary>(
      boost::make_tuple(tab.id, o)
   );

   BOOST_REQUIRE( kv_obj != nullptr );

   return fc::raw::unpack<Object>(
      kv_obj->value.data(),
      kv_obj->value.size()
   );
}

template <typename T, typename Object>
static std::optional<Object> get_by_index(chainbase::database& db, const name& scope, const name& inx, const T& o) {
   
   const auto& tab_inx = find_or_create_table(
      db, "evm"_n, scope, Object::index_name(inx) , "evm"_n
   );

   auto k256_value = to_key256(o);

   const auto* i256_obj = db.find<index256_object, by_secondary>(
      boost::make_tuple(tab_inx.id, k256_value)
   );

   if( i256_obj == nullptr ) return {};
   BOOST_REQUIRE( k256_value == i256_obj->secondary_key );

   const auto& tab = find_or_create_table(
      db, "evm"_n, scope, Object::table_name(), "evm"_n
   );

   const auto* kv_obj = db.find<key_value_object, by_scope_primary>(
      boost::make_tuple(tab.id, i256_obj->primary_key)
   );

   BOOST_REQUIRE( kv_obj != nullptr );
   BOOST_REQUIRE( i256_obj->primary_key == kv_obj->primary_key );

   return fc::raw::unpack<Object>(
      kv_obj->value.data(),
      kv_obj->value.size()
   );
}


key256_t to_key256(const uint8_t* ptr, size_t len) {
   uint8_t buffer[32]={0};
   BOOST_REQUIRE(len <= sizeof(buffer));
   memcpy(buffer, ptr, len);
   checksum256 cm(buffer);
   return chain::key256_t(cm.get_array());
}

key256_t to_key256(const evmc::address& addr) {
   return to_key256(addr.bytes, sizeof(addr.bytes));
}

key256_t to_key256(const evmc::bytes32& data){
    return to_key256(data.bytes, sizeof(data.bytes));
}

key256_t to_key256(const bytes& data){
    return to_key256((const uint8_t*)data.data(), data.size());
}

bytes to_bytes(const u256& val) {
    uint8_t tmp[32];
    intx::be::store(tmp, val);
    return bytes{tmp, std::end(tmp)};
}

bytes to_bytes(const Bytes& b) {
    return bytes{b.begin(), b.end()};
}

bytes to_bytes(const ByteView& b) {
    return bytes{b.begin(), b.end()};
}

bytes to_bytes(const evmc::bytes32& val) {
    return bytes{val.bytes, std::end(val.bytes)};
}

bytes to_bytes(const evmc::address& addr) {
    return bytes{addr.bytes, std::end(addr.bytes)};
}

bytes to_bytes(const key256_t& k) {
    checksum256 tmp(k);
    auto b = tmp.extract_as_byte_array();
    //memcpy(tmp, &c, 32);
    return bytes{b.begin(), b.end()};
}

evmc::address to_address(const bytes& addr) {
    evmc::address res;
    memcpy(res.bytes, addr.data(), sizeof(res.bytes));
    return res;
}

struct block_info {
   bytes    coinbase;
   uint64_t difficulty;
   uint64_t gasLimit;
   uint64_t number;
   uint64_t timestamp;
   //fc::optional<bytes> base_fee_per_gas;

   static block_info create(const Block& block) {
      block_info bi;
      bi.coinbase   = to_bytes(block.header.beneficiary);
      bi.difficulty = static_cast<uint64_t>(block.header.difficulty);
      bi.gasLimit   = block.header.gas_limit;
      bi.number     = block.header.number;
      bi.timestamp  = block.header.timestamp;

      // if(block.header.base_fee_per_gas.has_value())
      //    bi.base_fee_per_gas = to_bytes(*block.header.base_fee_per_gas);
      
      return bi;
    }
};
//FC_REFLECT(block_info, (coinbase)(difficulty)(gasLimit)(number)(timestamp)(base_fee_per_gas));
FC_REFLECT(block_info, (coinbase)(difficulty)(gasLimit)(number)(timestamp));

struct account {
   uint64_t    id;
   bytes       eth_address;
   uint64_t    nonce;
   bytes       balance;
   std::optional<uint64_t>       code_id;


   struct by_address {
      typedef index256_object index_object;
      static name index_name() {
         return account::index_name("by.address"_n);
      }
   };

   evmc::uint256be get_balance()const {
      evmc::uint256be res;
      std::copy(balance.begin(), balance.end(), res.bytes);
      return res;
   }

   static name table_name() { return "account"_n; }
   static name index_name(const name& n) {
      uint64_t index_table_name = table_name().to_uint64_t() & 0xFFFFFFFFFFFFFFF0ULL;

      return name{index_table_name | 0};
   }

   static name index_name(uint64_t n) {
      return index_name(name{n});
   }

   static std::optional<account> get_by_address(chainbase::database& db, const evmc::address& address) {
      auto r = get_by_index<evmc::address, account>(db, "evm"_n, "by.address"_n, address);
      return r;
   }

};
FC_REFLECT(account, (id)(eth_address)(nonce)(balance)(code_id));

struct account_code {
   uint64_t    id;
   uint32_t    ref_count;
   bytes       code;
   bytes       code_hash;


   struct by_codehash {
      typedef index256_object index_object;
      static name index_name() {
         return account_code::index_name("by.codehash"_n);
      }
   };

   evmc::bytes32 get_code_hash()const {
      evmc::bytes32 res;
      std::copy(code_hash.begin(), code_hash.end(), res.bytes);
      return res;
   }

   static name table_name() { return "accountcode"_n; }
   static name index_name(const name& n) {
      uint64_t index_table_name = table_name().to_uint64_t() & 0xFFFFFFFFFFFFFFF0ULL;

     return name{index_table_name | 0};
   }

   static name index_name(uint64_t n) {
      return index_name(name{n});
   }

   static std::optional<account_code> get_by_code_hash(chainbase::database& db, const evmc::bytes32& code_hash) {
      auto r = get_by_index<evmc::bytes32, account_code>(db, "evm"_n, "by.codehash"_n, code_hash);
      return r;
   }

};
FC_REFLECT(account_code, (id)(ref_count)(code)(code_hash));

struct storage {
   uint64_t id;
   bytes    key;
   bytes    value;

   struct by_key {
      typedef index256_object index_object;
      static name index_name() {
         return storage::index_name("by.key"_n);
      }
   };

   evmc::bytes32 get_value() {
      evmc::bytes32 res;
      memcpy(res.bytes, value.data(), value.size());
      return res;
   }

   static name table_name() { return "storage"_n; }
   static name index_name(const name& n) {
      uint64_t index_table_name = table_name().to_uint64_t() & 0xFFFFFFFFFFFFFFF0ULL;

      //0=>by.key
      if( n == "by.key"_n ) {
         return name{index_table_name | 0};
      }

      BOOST_REQUIRE(false);
      return name{0};
   }

   static name index_name(uint64_t n) {
      return index_name(name{n});
   }

   static std::optional<storage> get(chainbase::database& db, uint64_t account, const evmc::bytes32& key) {
      return get_by_index<evmc::bytes32, storage>(db, name{account}, "by.key"_n, key);
   }

};
FC_REFLECT(storage, (id)(key)(value));

struct gcstore {
   uint64_t id;
   uint64_t storage_id;

   static name table_name() { return "gcstore"_n; }
   static name index_name(const name& n) {
      BOOST_REQUIRE(false);
      return name{0};
   }

   static name index_name(uint64_t n) {
      return index_name(name{n});
   }
};
FC_REFLECT(gcstore, (id)(storage_id));

struct assert_message_check {
   string _expected;
   assert_message_check(const string& expected) {
      _expected = expected;
   }

   bool operator()(const fc::exception& ex) {
      return eosio::testing::expect_assert_message(ex, _expected);
   }
};

struct evm_runtime_tester : eosio_system_tester, silkworm::State {
   
   abi_serializer evm_runtime_abi;
   std::map< name, private_key> key_map;
   bool is_verbose = false;
   bool slow_tests = false;

   size_t total_passed{0};
   size_t total_failed{0};
   size_t total_skipped{0};

   evm_runtime_tester() {
      std::string verbose_arg = "--verbose";
      std::string slowtests_arg = "--slow-tests";
      auto argc = boost::unit_test::framework::master_test_suite().argc;
      auto argv = boost::unit_test::framework::master_test_suite().argv;
      for (int i = 0; i < argc; i++) {
         if (verbose_arg == argv[i]) {
            is_verbose = true;
         }
         if (slowtests_arg == argv[i]) {
            slow_tests = true;
         }
      }

      BOOST_REQUIRE_EQUAL( success(), push_action(eosio::chain::config::system_account_name, "wasmcfg"_n, mvo()("settings", "high")) );
      create_account_with_resources(ME, system_account_name, 6000000);
      set_authority( ME, "active"_n, {1, {{get_public_key(ME,"active"),1}}, {{{ME,"eosio.code"_n},1}}} );

      set_code(ME, contracts::evm_runtime_wasm());
      set_abi(ME, contracts::evm_runtime_abi().data());

      const auto& accnt = control->db().get<account_object,by_name>(ME);
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      evm_runtime_abi.set_abi(abi, abi_serializer_max_time);

      base_tester::push_action(ME,
                               "init"_n,
                               ME,
                               mvo()                                   //
                               ("chainid", 15555)                      //
                               ("fee_params",                          //
                                mvo()                                  //
                                ("gas_price", 150'000'000'000)         //
                                ("miner_cut", 10'000)                  //
                                ("ingress_bridge_fee", fc::variant())) //
      );
   }

   std::string to_str(const fc::variant& o) {
      return fc::json::to_pretty_string(o, fc::time_point(fc::time_point::now()+abi_serializer_max_time) );
   }

   action_result call( const name signer, const action_name &name, const variant_object &data ) {

      string action_type_name = evm_runtime_abi.get_action_type(name);

      vector<action> acts;
   
      action act;
      act.authorization.push_back(permission_level{signer, "active"_n});
      act.account       = ME;
      act.name          = name;
      
      if( evm_runtime_abi.get_action_type(name) != type_name())
         act.data = evm_runtime_abi.variant_to_binary( action_type_name, data, abi_serializer_max_time );

      acts.emplace_back(std::move(act));
      return my_push_action(std::move(acts));
   }

   static inline void print_debug(const action_trace& ar) {
      if (!ar.console.empty()) {
         cout << ": xCONSOLE OUTPUT BEGIN =====================" << endl
            << ar.console << endl
            << ": CONSOLE OUTPUT END   =====================" << endl;
      }
   }

   transaction_trace_ptr last_tx_trace;
   action_result my_push_action(vector<action>&& acts) {
      signed_transaction trx;
      trx.actions = std::move(acts);

      auto call_info = fc::format_string(
         "[(${a},${n})]",
         fc::mutable_variant_object()
            ("a", trx.actions[0].account)
            ("n", trx.actions[0].name)
      );
      dlog("calling: ${i}", ("i",call_info));

      set_transaction_headers(trx);
      for(const auto& act : trx.actions) {
         for(const auto& perm: act.authorization) {
            trx.sign(get_private_key(perm.actor, perm.permission.to_string()), control->get_chain_id());
         }
      }

      try {
         last_tx_trace = push_transaction(trx);
         // if(is_verbose) {
         //    print_debug(last_tx_trace->action_traces[0]);
         // }
      } catch (const fc::exception& ex) {
         if(is_verbose) {
            edump((ex));
            edump((ex.to_detail_string()));
            // cout << "-----EXCEPTION------" << endl
            //      << "HEX: " << fc::json::to_string(trx.actions[0].data, fc::time_point(fc::time_point::now()+abi_serializer_max_time)) << endl
            //      << fc::json::to_string(ex, fc::time_point(fc::time_point::now()+abi_serializer_max_time)) << endl << endl;
         }
         return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
      } catch (...) {
         elog("unhandled exception in test");
         return error("unhandled exception in test");
      }
      produce_block();
      BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
      return success();
   }

   //------ actions
   
   action_result clearall(name signer=ME ) { 
      return call(signer, "clearall"_n, mvo()
      );
   }

   action_result gc( uint32_t max, name signer=ME ) {
      return call(signer, "gc"_n, mvo()
                  ("max", max)
      );
   }

   action_result dumpstorage(const bytes& address, name signer=ME ) { 
      return call(signer, "dumpstorage"_n, mvo()
         ("addy", address)
      );
   }

   action_result dumpall(name signer=ME ) { 
      return call(signer, "dumpall"_n, {});
   }
   
   action_result updatestore( const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current, name signer=ME ) { 
      return call(signer, "updatestore"_n, mvo()
         ("address", address)
         ("incarnation", incarnation)
         ("location", location)
         ("initial", initial)
         ("current", current)
      );
   }

   action_result updateaccnt( const bytes& address, const bytes& initial, const bytes& current, name signer=ME ) { 
      return call(signer, "updateaccnt"_n, mvo()
         ("address", address)
         ("initial", initial)
         ("current", current)
      );
   }

   action_result updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code, name signer=ME ) { 
      return call(signer, "updatecode"_n, mvo()
         ("address", address)
         ("incarnation", incarnation)
         ("code_hash", code_hash)
         ("code", code)
      );
   }

   action_result pushtx( const std::optional<bytes>& rlptx, const block_info& bi, name signer=ME ) { 
      return call(signer, "testtx"_n, mvo()
         ("orlptx", rlptx)
         ("bi", bi)
      );
   }

   action_result addone( const bytes& addy, name signer=ME ) { 
      return call(signer, "addone"_n, mvo()
         ("addy", addy)
      );
   }

   action_result testbaldust( name testname ) {
      return call(ME, "testbaldust"_n, mvo()
         ("test", testname)
      );
   }

   //------ silkworm state impl
   std::optional<Account> read_account(const evmc::address& address) const noexcept {
      auto& db = const_cast<chainbase::database&>(control->db());
      auto accnt = account::get_by_address(db, address);
      if(!accnt) return {};

      if (accnt->code_id.has_value()) {
         auto r = get_by_primary_key<uint64_t, account_code>(db, "evm"_n, accnt->code_id.value());
         if (r) {
            evmc::bytes32 res;
            std::copy(r.value().code_hash.begin(), r.value().code_hash.end(), res.bytes);
            return Account{
               accnt->nonce,
               intx::be::load<u256>(accnt->get_balance()),
               res,
               0 //TODO: ??
            };
         }
      }
      return Account{
            accnt->nonce,
            intx::be::load<u256>(accnt->get_balance()),
            kEmptyHash,
            0 //TODO: ??
      };
   };

   mutable bytes read_code_buffer;
   ByteView read_code(const evmc::bytes32& code_hash) const noexcept {
      auto& db = const_cast<chainbase::database&>(control->db());
      auto accntcode = account_code::get_by_code_hash(db, code_hash);
      if(!accntcode) {
         dlog("no code for hash ${ch}", ("ch",to_bytes(code_hash)));
         return ByteView{};
      }
      //dlog("${a} ${c} ${ch} ${ch2}", ("a",accnt->eth_address)("c",accnt->code)("ch2",accnt->code_hash)("ch",to_bytes(code_hash)));
      read_code_buffer = accntcode->code;
      return ByteView{(const uint8_t*)read_code_buffer.data(), read_code_buffer.size()};
   }

   evmc::bytes32 read_storage(const evmc::address& address, uint64_t incarnation,
                                    const evmc::bytes32& location) const noexcept {
      auto& db = const_cast<chainbase::database&>(control->db());
      auto accnt = account::get_by_address(db, address);
      if(!accnt) return {};
      auto s = storage::get(db, accnt->id, location);
      if(!s) return {};
      return s->get_value();
   }

   /** Previous non-zero incarnation of an account; 0 if none exists. */
   uint64_t previous_incarnation(const evmc::address& address) const noexcept {
      return 0;
   };

   evmc::bytes32 state_root_hash() const {
      return {};   
   };

   uint64_t current_canonical_block() const {
      return 0;
   };

   std::optional<evmc::bytes32> canonical_hash(uint64_t block_number) const {
      return {};   
   };

   void insert_block(const Block& block, const evmc::bytes32& hash) {
      return;
   };

   void canonize_block(uint64_t block_number, const evmc::bytes32& block_hash) {
      return;
   };

   void decanonize_block(uint64_t block_number) {
      return;
   };

   void insert_receipts(uint64_t block_number, const std::vector<Receipt>& receipts) {
      return;
   };

   void begin_block(uint64_t block_number) {
   };

   void update_account(const evmc::address& address, std::optional<Account> initial,
                              std::optional<Account> current) {
      
      bytes oinitial;
      if(initial.has_value()) {
         oinitial = to_bytes(initial->encode_for_storage());
      }

      bytes ocurrent;
      if(current.has_value()) {
         ocurrent = to_bytes(current->encode_for_storage());
      }

      updateaccnt(to_bytes(address), oinitial, ocurrent);
   }

   void update_account_code(const evmc::address& address, uint64_t incarnation, const evmc::bytes32& code_hash,
                                    ByteView code) {
      updatecode(to_bytes(address), incarnation, to_bytes(code_hash), to_bytes(code));
   };

   void update_storage(const evmc::address& address, uint64_t incarnation, const evmc::bytes32& location,
                              const evmc::bytes32& initial, const evmc::bytes32& current) {

      updatestore(to_bytes(address), incarnation, to_bytes(location), to_bytes(initial), to_bytes(current));
   };

   void unwind_state_changes(uint64_t block_number) {
      BOOST_REQUIRE(false);
   }

   std::optional<BlockHeader> read_header(BlockNum block_number,
                                             const evmc::bytes32& block_hash) const noexcept {
      return {};
   }

    // Returns true on success and false on missing block
   bool read_body(BlockNum block_number, const evmc::bytes32& block_hash,
                                         BlockBody& out) const noexcept {
      return false;
   };


   std::optional<intx::uint256> total_difficulty(uint64_t block_number,
                                             const evmc::bytes32& block_hash) const noexcept {
      return {};
   }

   size_t number_of_accounts() {
      auto& db = const_cast<chainbase::database&>(control->db());

      const auto* tid = db.find<table_id_object, by_code_scope_table>(
         boost::make_tuple("evm"_n, "evm"_n,"account"_n)
      );

      if(tid == nullptr) return 0;

      const auto& idx = db.get_index<key_value_index, by_scope_primary>();
      auto itr = idx.lower_bound( boost::make_tuple( tid->id) );
      size_t count=0;
      while ( itr != idx.end() && itr->t_id == tid->id ) {
         ++itr;
         ++count;
      }
      return count;
   }

   size_t state_storage_size(const evmc::address& address, uint64_t incarnation) {
      auto& db = const_cast<chainbase::database&>(control->db());
      auto accnt = account::get_by_address(db, address);
      if(!accnt) {
         dlog("${a} => 0",("a",to_bytes(address)));
         return 0;
      }

      //dlog("${a}", ("a",*accnt));
      const auto* tid = db.find<table_id_object, by_code_scope_table>(
         boost::make_tuple("evm"_n, name{accnt->id}, storage::table_name())
      );

      if(tid == nullptr) {
         dlog("${a} => 0 (bis)",("a",to_bytes(address)));
         return 0;
      }

      fc::mutable_variant_object mr;
      mr("a",to_bytes(address));
      //std::cout << "state_storage_size: " << fc::format_string("${a}",mr,true) << std::endl;

      const auto& idx = db.get_index<key_value_index, by_scope_primary>();
      auto itr = idx.lower_bound( boost::make_tuple(tid->id) );
      size_t count=0;
      while ( itr != idx.end() && itr->t_id == tid->id ) {

         auto r = fc::raw::unpack<storage>(
            itr->value.data(),
            itr->value.size()
         );

         // fc::mutable_variant_object mu;
         // mu( "a", r.key );
         // mu( "b", r.value );
         // std::cout << fc::format_string("   ${a}=${b}", mu, true) << std::endl;
         
         ++itr;
         ++count;
      }
      //dlog("${a} => ${c}",("a",to_bytes(address))("c",count));
      //std::cout << "   total: " << count << std::endl;
      return count;
   }

   size_t gc_size() {
      auto& db = const_cast<chainbase::database&>(control->db());
      const auto* tid = db.find<table_id_object, by_code_scope_table>(
          boost::make_tuple("evm"_n, "evm"_n, gcstore::table_name())
      );

      if(tid == nullptr) {
         return 0;
      }

      const auto& idx = db.get_index<key_value_index, by_scope_primary>();
      auto itr = idx.lower_bound( boost::make_tuple(tid->id) );
      size_t count=0;
      while ( itr != idx.end() && itr->t_id == tid->id ) {
         ++itr;
         ++count;
      }
      return count;
   }

   //----
   std::vector<fs::path> excluded_tests;
   std::vector<fs::path> included_tests;
   bool exclude_test(const fs::path& p, const fs::path& root_dir, bool include_slow_tests) {
      const auto path_fits = [&p, &root_dir](const fs::path& e) { 
         return root_dir / e == p; 
      };

      return !as_range::any_of(included_tests, path_fits) && as_range::any_of(excluded_tests, path_fits) ||
            (!include_slow_tests && as_range::any_of(kSlowTests, path_fits));
   }

   void load_excluded() {
      if ( !fs::is_regular_file(contracts::skip_list()) ) {
         dlog("skip list not found");
         return;
      }
      
      boost::filesystem::ifstream fileHandler(contracts::skip_list());
      string line;
      while (getline(fileHandler, line)) {
         boost::trim(line);
         if(!line.length() || boost::starts_with(line,"#")) continue;
         if(boost::starts_with(line,"%")) {
            included_tests.emplace_back(fs::path(line.substr(1)));
         } else {
            excluded_tests.emplace_back(fs::path(line));
         }
      }

      for(auto& i : included_tests) {
         std::cout << "force: " << i << std::endl;
      }
   }

   ValidationResult apply_test_block(const Block& block) {
      
      auto bi = block_info::create(block);
      dlog("txs: ${n}, block_info ${a}", ("n",block.transactions.size())("a",bi));

      if(block.transactions.empty()) {
         auto res = pushtx({}, bi);
         if(res.size()) {
            std::cout << "ERR:" << res << std::endl;
            return ValidationResult::kInvalidOmmerHeader;
         }
      } else {
         for(const auto& tx : block.transactions) {
            Bytes btx;
            rlp::encode(btx, tx);
            auto res = pushtx(to_bytes(btx), bi);
            if(res.size()) {
               std::cout << "ERR:" << res << std::endl;
               return ValidationResult::kInvalidOmmerHeader;
            }
         }
      }

      return ValidationResult::kOk;
   }

   Status run_block(const nlohmann::json& json_block) {
      bool invalid{json_block.contains("expectException")};

      std::optional<Bytes> rlp{from_hex(json_block["rlp"].get<std::string>())};
      if (!rlp) {
         if (invalid) {
               dlog("invalid=kPassed 1");
               return Status::kPassed;
         }
         std::cout << "Failure to read hex" << std::endl;
         return Status::kFailed;
      }

      Block block;
      ByteView view{*rlp};
      if (rlp::decode(view, block) != DecodingResult::kOk || !view.empty()) {
         if (invalid) {
               dlog("invalid=kPassed 2");
               return Status::kPassed;
         }
         std::cout << "Failure to decode RLP" << std::endl;
         return Status::kFailed;
      }

      bool check_state_root{invalid && json_block["expectException"].get<std::string>() == "InvalidStateRoot"};
      
      if (ValidationResult err{apply_test_block(block)}; err != ValidationResult::kOk) {
         if (invalid) {
               dlog("invalid=kPassed 3");
               return Status::kPassed;
         }
         std::cout << "Validation error " << magic_enum::enum_name<ValidationResult>(err) << std::endl;
         return Status::kFailed;
      }

      if (invalid) {
         std::cout << "Invalid block executed successfully\n";
         std::cout << "Expected: " << json_block["expectException"] << std::endl;
         return Status::kFailed;
      }

      return Status::kPassed;
   }

   // https://ethereum-tests.readthedocs.io/en/latest/test_types/blockchain_tests.html#pre-prestate-section
   void init_pre_state(const std::string& test_name, const nlohmann::json& pre) {
      for (const auto& entry : pre.items()) {
         const evmc::address address{to_evmc_address(from_hex(entry.key()).value())};
         const nlohmann::json& j{entry.value()};

         Account account;
         const auto balance{intx::from_string<intx::uint256>(j["balance"].get<std::string>())};
         account.balance = balance;
         const auto nonce_str{j["nonce"].get<std::string>()};
         account.nonce = std::stoull(nonce_str, nullptr, /*base=*/16);

         update_account(address, /*initial=*/std::nullopt, account);

         const Bytes code{from_hex(j["code"].get<std::string>()).value()};
         if (!code.empty()) {
               account.incarnation = kDefaultIncarnation;
               auto c = fc::to_hex((const char*)code.data(), code.size());
               account.code_hash = bit_cast<evmc_bytes32>(ethash::keccak256(code.data(), code.size()));
               auto ch = fc::to_hex((const char*)account.code_hash.bytes, 32);
               
               update_account_code(address, account.incarnation, account.code_hash, code);
         }

         for (const auto& storage : j["storage"].items()) {
               Bytes key{from_hex(storage.key()).value()};
               Bytes value{from_hex(storage.value().get<std::string>()).value()};
               update_storage(address, account.incarnation, to_bytes32(key), /*initial=*/{}, to_bytes32(value));
         }
      }
   }

   bool post_check(const nlohmann::json& expected) {
      if (number_of_accounts() != expected.size()) {
         std::cout << "Account number mismatch: " << number_of_accounts() << " != " << expected.size()
                     << std::endl;
         return false;
      }

      for (const auto& entry : expected.items()) {
         const evmc::address address{to_evmc_address(from_hex(entry.key()).value())};
         const nlohmann::json& j{entry.value()};

         std::optional<Account> account{read_account(address)};
         if (!account) {
               std::cout << "Missing account " << entry.key() << std::endl;
               return false;
         }

         const auto expected_balance{intx::from_string<intx::uint256>(j["balance"].get<std::string>())};
         // std::cout << "comparing balance for " << entry.key() << ":\n"
         //          << intx::to_string(account->balance, 16) << "," << j["balance"] << std::endl;

         if (account->balance != expected_balance) {
               std::cout << "Balance mismatch for " << entry.key() << ":\n"
                        << intx::to_string(account->balance, 16) << " != " << j["balance"] << std::endl;
               return false;
         }

         const auto expected_nonce{intx::from_string<intx::uint256>(j["nonce"].get<std::string>())};
         if (account->nonce != expected_nonce) {
               std::cout << "Nonce mismatch for " << entry.key() << ":\n"
                        << account->nonce << " != " << j["nonce"] << std::endl;
               return false;
         }

         auto expected_code{j["code"].get<std::string>()};
         Bytes actual_code{read_code(account->code_hash)};
         if (actual_code != from_hex(expected_code)) {
               std::cout << "Code mismatch for " << entry.key() << "\n";
                        //<< to_hex(actual_code) << " != " << expected_code << std::endl;
               return false;
         }

         size_t storage_size{state_storage_size(address, account->incarnation)};
         if (storage_size != j["storage"].size()) {
               std::cout << "Storage size mismatch for " << entry.key() << ":\n"
                        << storage_size << " != " << j["storage"].size() << std::endl;
               return false;
         }

         for (const auto& storage : j["storage"].items()) {
               Bytes key{from_hex(storage.key()).value()};
               Bytes expected_value{from_hex(storage.value().get<std::string>()).value()};
               evmc::bytes32 actual_value{read_storage(address, account->incarnation, to_bytes32(key))};
               if (actual_value != to_bytes32(expected_value)) {
                  std::cout << "Storage mismatch for " << entry.key() << " at " << storage.key() << ":\n"
                           << to_hex(actual_value) << " != " << to_hex(expected_value) << std::endl;
                  return false;
               }
         }
      }

      if( gc_size() != 0 ) {
         std::cout << "gcstore is not empty: " << gc_size() << std::endl;
         return false;
      }

      return true;
   }


   // https://ethereum-tests.readthedocs.io/en/latest/test_types/blockchain_tests.html
   RunResults blockchain_test(const std::string& test_name, const nlohmann::json& json_test) {

      //mod_exp restriction: exponent bit size cannot exceed bit size of either base or modulus
      if( test_name == "modexp_d27g0v0_Istanbul" ||
          test_name == "modexp_d27g1v0_Istanbul" ||
          test_name == "modexp_d27g2v0_Istanbul" ||
          test_name == "modexp_d27g3v0_Istanbul" ) {
         return Status::kSkipped;
      }

      if (json_test.contains("postStateHash")) {
         return Status::kSkipped;
      }

      std::string network{json_test["network"].get<std::string>()};
      init_pre_state(test_name, json_test["pre"]);

      for (const auto& json_block : json_test["blocks"]) {
         Status status{run_block(json_block)};
         if (status != Status::kPassed) {
               return status;
         }
      }

      gc(std::numeric_limits<uint32_t>::max());

      if (post_check(json_test["postState"])) {
         return Status::kPassed;
      } else {
         return Status::kFailed;
      }
   }

   void run_test_file(const fs::path& file_path, RunnerFunc runner) {
      std::ifstream in{file_path.string()};
      nlohmann::json json;

      try {
         in >> json;
      } catch (nlohmann::detail::parse_error& e) {
         std::cerr << e.what() << "\n";
         print_test_status(file_path.string(), Status::kSkipped);
         ++total_skipped;
         return;
      }

      RunResults total;

      for (const auto& test : json.items()) {
         auto json_test = test.value();
         std::string network{json_test["network"].get<std::string>()};

         //Only Istanbul
         if(network != "Istanbul") continue;

         const RunResults r{(*this.*runner)(test.key(), json_test)};
         total += r;
         if (r.failed || r.skipped) {
               print_test_status(test.key(), r);
         }
         
         clearall();
      }

      total_passed += total.passed;
      total_failed += total.failed;
      total_skipped += total.skipped;
   }

   static void print_test_status(std::string_view key, const RunResults& res) {
      std::cout << key << " ";
      for (size_t i{key.length() + 1}; i < kColumnWidth; ++i) {
         std::cout << '.';
      }
      if (res.failed) {
         std::cout << "  Failed" << std::endl;
      } else if (res.skipped) {
         std::cout << " Skipped" << std::endl;
      } else {
         std::cout << "  Passed" << std::endl;
      }
   }

};

BOOST_AUTO_TEST_SUITE(evm_runtime_tests)
BOOST_FIXTURE_TEST_CASE( GeneralStateTests, evm_runtime_tester ) try {

   StopWatch sw;
   sw.start();

   load_excluded();

   const fs::path root_dir{contracts::eth_test_folder()};

   static const std::map<fs::path, RunnerFunc> kTestTypes{
      {kBlockchainDir, &evm_runtime_tester::blockchain_test},
      //{kTransactionDir, transaction_test},
   };

   for (const auto& entry : kTestTypes) {
      const fs::path& dir{entry.first};
      const RunnerFunc runner{entry.second};

      for (auto i = fs::recursive_directory_iterator(root_dir / dir); i != fs::recursive_directory_iterator{}; ++i) {
         if (exclude_test(*i, root_dir, slow_tests)) {
               ++total_skipped;
               i.disable_recursion_pending();
         } else if (fs::is_regular_file(i->path())) {
               const fs::path path{*i};
               run_test_file(path, runner);
         }
      }
   }

   const auto [_, duration] = sw.lap();
   std::cout << total_passed  << " tests passed" << ", "
             << total_failed  << " failed" << ", "
             << total_skipped << " skipped"
             << " in " << StopWatch::format(duration) << std::endl;

   BOOST_REQUIRE_EQUAL(total_failed, 0);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( balance_and_dust_tests, evm_runtime_tester ) try {
   BOOST_REQUIRE_EQUAL(testbaldust("basic"_n),      success());

   BOOST_REQUIRE_EQUAL(testbaldust("underflow1"_n), error("assertion failure with message: decrementing more than available"));
   BOOST_REQUIRE_EQUAL(testbaldust("underflow2"_n), error("assertion failure with message: decrementing more than available"));
   BOOST_REQUIRE_EQUAL(testbaldust("underflow3"_n), error("assertion failure with message: decrementing more than available"));
   BOOST_REQUIRE_EQUAL(testbaldust("underflow4"_n), error("assertion failure with message: decrementing more than available"));
   BOOST_REQUIRE_EQUAL(testbaldust("underflow5"_n), error("assertion failure with message: decrementing more than available"));

   BOOST_REQUIRE_EQUAL(testbaldust("overflow1"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflow2"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflow3"_n),  success());
   BOOST_REQUIRE_EQUAL(testbaldust("overflow4"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflow5"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflowa"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflowb"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflowc"_n),  error("assertion failure with message: accumulation overflow"));
   BOOST_REQUIRE_EQUAL(testbaldust("overflowd"_n),  error("assertion failure with message: accumulation overflow"));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
