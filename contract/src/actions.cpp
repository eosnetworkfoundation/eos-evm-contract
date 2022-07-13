#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/processor.hpp>
#include <evm_runtime/state.hpp>
#include <evm_runtime/engine.hpp>
#include <evm_runtime/config.hpp>
#include <evm_runtime/intrinsics.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/engine.hpp>
#include <evm_runtime/test/config.hpp>
#endif

#ifdef WITH_LOGTIME
#define LOGTIME(MSG) eosio::internal_use_do_not_use::logtime(MSG)
#else
#define LOGTIME(MSG)
#endif

namespace evm_runtime {

using namespace silkworm;

db_stats evm_contract::pushtx( eosio::name ram_payer, const bytes& rlptx ) {
    LOGTIME("EVM START");
    eosio::require_auth(ram_payer);

    Block block;
    block.header.difficulty  = 0;
    block.header.gas_limit   = INT64_MAX;
    block.header.number      = eosio::internal_use_do_not_use::get_block_num();
    block.header.timestamp   = eosio::current_time_point().sec_since_epoch();

    Transaction tx;
    ByteView bv{(const uint8_t*)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == rlp::DecodingResult::kOk && bv.empty(), "unable to decode transaction");
    LOGTIME("EVM TX DECODE");

    tx.from.reset();
    tx.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");
    LOGTIME("EVM RECOVER SENDER");

    evm_runtime::engine engine;
    evm_runtime::state state{get_self(), ram_payer};
    evm_runtime::ExecutionProcessor ep{block, engine, state, kJungle4};

    Receipt receipt;
    ep.execute_transaction(tx, receipt);
    
    LOGTIME("EVM EXECUTE");
    return state.stats;
}

#ifdef WITH_TEST_ACTIONS
ACTION evm_contract::testtx( const bytes& rlptx, const evm_runtime::test::block_info& bi ) {
    eosio::require_auth(get_self());
    
    Block block;
    block.header = bi.get_block_header();

    Transaction tx;
    ByteView bv{(const uint8_t *)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == rlp::DecodingResult::kOk && bv.empty(), "unable to decode transaction");

    tx.from.reset();
    tx.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");

    evm_runtime::test::engine engine;
    evm_runtime::state state{get_self(), get_self()};
    evm_runtime::ExecutionProcessor ep{block, engine, state, evm_runtime::test::kTestNetwork};

    Receipt receipt;
    ep.execute_transaction(tx, receipt);
}

ACTION evm_contract::dumpstorage(const bytes& addy) {
    eosio::require_auth(get_self());
    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(to_address(addy)));
    if(itr == inx.end()) {
        eosio::print("no data for: ");
        eosio::printhex(addy.data(), addy.size());
        eosio::print("\n");
        return;
    }

    eosio::print("storage: ");
    eosio::printhex(addy.data(), addy.size());

    uint64_t cnt=0;
    storage_table db(_self, itr->id);
    auto sitr = db.begin();
    while(sitr != db.end()) {
        eosio::print("\n");
        eosio::printhex(sitr->key.data(), sitr->key.size());
        eosio::print(":");
        eosio::printhex(sitr->value.data(), sitr->value.size());
        eosio::print("\n");
        ++sitr;
        ++cnt;
    }

    eosio::print(" = ", cnt, "\n");
}

ACTION evm_contract::dumpall() {
    eosio::require_auth(get_self());
    account_table accounts(_self, _self.value);
    auto itr = accounts.begin();
    eosio::print("DUMPALL start\n");
    while( itr != accounts.end() ) {
        eosio::print("  account:");
        eosio::printhex(itr->eth_address.data(), itr->eth_address.size());
        eosio::print("\n");
        storage_table db(_self, itr->id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            eosio::print("    ");
            eosio::printhex(sitr->key.data(), sitr->key.size());
            eosio::print(":");
            eosio::printhex(sitr->value.data(), sitr->value.size());
            eosio::print("\n");
            sitr++;
        }
        
        itr++;
    }
    eosio::print("DUMPALL end\n");
}


ACTION evm_contract::clearall() {
    eosio::require_auth(get_self());
    account_table accounts(_self, _self.value);
    auto itr = accounts.begin();
    eosio::print("CLEAR start\n");
    while( itr != accounts.end() ) {
        eosio::print("  account:");
        eosio::printhex(itr->eth_address.data(), itr->eth_address.size());
        eosio::print("\n");
        storage_table db(_self, itr->id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            eosio::print("    ");
            eosio::printhex(sitr->key.data(), sitr->key.size());
            eosio::print(":");
            eosio::printhex(sitr->value.data(), sitr->value.size());
            eosio::print("\n");
            sitr = db.erase(sitr);
        }

        auto db_size = std::distance(db.cbegin(), db.cend());
        eosio::print("db size:", uint64_t(db_size), "\n");
        itr = accounts.erase(itr);
    }

    auto account_size = std::distance(accounts.cbegin(), accounts.cend());
    eosio::print("accounts size:", uint64_t(account_size), "\n");

    eosio::print("CLEAR end\n");
}

ACTION evm_contract::updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code) {
    eosio::require_auth(get_self());
    evm_runtime::state state{get_self(), get_self()};
    auto bvcode = ByteView{(const uint8_t *)code.data(), code.size()};
    state.update_account_code(to_address(address), incarnation, to_bytes32(code_hash), bvcode);
}

ACTION evm_contract::updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current) {
    eosio::require_auth(get_self());
    evm_runtime::state state{get_self(), get_self()};
    eosio::print("updatestore: ");
    eosio::printhex(address.data(), address.size());
    eosio::print("\n   ");
    eosio::printhex(location.data(), location.size());
    eosio::print(":");
    eosio::printhex(current.data(), current.size());
    eosio::print("\n");
    
    state.update_storage(to_address(address), incarnation, to_bytes32(location), to_bytes32(initial), to_bytes32(current));
}

ACTION evm_contract::updateaccnt(const bytes& address, const bytes& initial, const bytes& current) {
    eosio::require_auth(get_self());
    evm_runtime::state state{get_self(), get_self()};
    auto maybe_account = [](const bytes& data) -> std::optional<Account> {
        std::optional<Account> res{};
        if(data.size()) {
            Account tmp;
            ByteView bv{(const uint8_t *)data.data(), data.size()};
            auto dec_res = decode_account_from_storage(bv);
            eosio::check(dec_res.second == rlp::DecodingResult::kOk, "unable to decode account");
            res = dec_res.first;
        }
        return res;
    };

    auto oinitial = maybe_account(initial);
    auto ocurrent = maybe_account(current);

    state.update_account(to_address(address), oinitial, ocurrent);
}

ACTION evm_contract::setbal(const bytes& addy, const bytes& bal) {
    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(addy));

    if(itr == inx.end()) {
        accounts.emplace(get_self(), [&](auto& row){
            row.id = accounts.available_primary_key();;
            row.code_hash = to_bytes(kEmptyHash);
            row.eth_address = addy;
            row.balance = bal;
        });
    } else {
        accounts.modify(*itr, eosio::same_payer, [&](auto& row){
            row.balance = bal;
        });
    }
}
#endif //WITH_TEST_ACTIONS

} //evm_runtime
