#include <eosio/system.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/state.hpp>
#include <evm_runtime/test/engine.hpp>
#include <evm_runtime/test/config.hpp>

namespace evm_runtime {
using namespace silkworm;

[[eosio::action]] void evm_contract::testtx( const std::optional<bytes>& orlptx, const evm_runtime::test::block_info& bi ) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    Block block;
    block.header = bi.get_block_header();

    evm_runtime::test::engine engine;
    evm_runtime::state state{get_self(), get_self()};
    silkworm::ExecutionProcessor ep{block, engine, state, evm_runtime::test::kTestNetwork};

    if(orlptx) {
        Transaction tx;
        ByteView bv{(const uint8_t*)orlptx->data(), orlptx->size()};
        eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");

        execute_tx(eosio::name{}, block, tx, ep, false);
    }
    engine.finalize(ep.state(), ep.evm().block(), ep.evm().revision());
    ep.state().write_to_db(ep.evm().block().header.number);
}

[[eosio::action]] void evm_contract::dumpstorage(const bytes& addy) {
    assert_inited();

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

[[eosio::action]] void evm_contract::dumpall() {
    assert_inited();

    eosio::require_auth(get_self());

    auto print_store = [](auto sitr) {
        eosio::print("    ");
        eosio::printhex(sitr->key.data(), sitr->key.size());
        eosio::print(":");
        eosio::printhex(sitr->value.data(), sitr->value.size());
        eosio::print("\n");
    };

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
            print_store( sitr );
            sitr++;
        }
        
        itr++;
    }
    eosio::print("  gc:");
    gc_store_table gc(_self, _self.value);
    auto i = gc.begin();
    while( i != gc.end() ) {
        eosio::print("   storage_id:");
        eosio::print(i->storage_id);
        eosio::print("\n");
        storage_table db(_self, i->storage_id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            print_store( sitr );
            ++sitr;
        }

        ++i;
    }

    eosio::print("DUMPALL end\n");
}

[[eosio::action]] void evm_contract::clearall() {
    assert_unfrozen();

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

    account_code_table codes(_self, _self.value);
    auto itrc = codes.begin();
    while(itrc != codes.end()) {
        itrc = codes.erase(itrc);
    }

    gc(std::numeric_limits<uint32_t>::max());

    auto account_size = std::distance(accounts.cbegin(), accounts.cend());
    eosio::print("accounts size:", uint64_t(account_size), "\n");

    eosio::print("CLEAR end\n");
}

[[eosio::action]] void evm_contract::updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto bvcode = ByteView{(const uint8_t *)code.data(), code.size()};
    state.update_account_code(to_address(address), incarnation, to_bytes32(code_hash), bvcode);
}

[[eosio::action]] void evm_contract::updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current) {
    assert_unfrozen();

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

[[eosio::action]] void evm_contract::updateaccnt(const bytes& address, const bytes& initial, const bytes& current) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto maybe_account = [](const bytes& data) -> std::optional<Account> {
        std::optional<Account> res{};
        if(data.size()) {
            Account tmp;
            ByteView bv{(const uint8_t *)data.data(), data.size()};
            auto dec_res = Account::from_encoded_storage(bv);
            eosio::check(dec_res.second == DecodingResult::kOk, "unable to decode account");
            res = dec_res.first;
        }
        return res;
    };

    auto oinitial = maybe_account(initial);
    auto ocurrent = maybe_account(current);

    state.update_account(to_address(address), oinitial, ocurrent);
}

[[eosio::action]] void evm_contract::setbal(const bytes& addy, const bytes& bal) {
    assert_unfrozen();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(addy));

    if(itr == inx.end()) {
        accounts.emplace(get_self(), [&](auto& row){
            row.id = accounts.available_primary_key();;
            row.code_id = std::nullopt;
            row.eth_address = addy;
            row.balance = bal;
        });
    } else {
        accounts.modify(*itr, eosio::same_payer, [&](auto& row){
            row.balance = bal;
        });
    }
}

[[eosio::action]] void evm_contract::testbaldust(const name test) {
    if(test == "basic"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b +=                      200000000_u256; //adds to dust only
        b +=                           3000_u256; //adds to dust only
        b +=                100000000000000_u256; //adds strictly to balance
        b +=                200000000007000_u256; //adds to both balance and dust
        b +=                 60000000000000_u256; //adds to dust only
        b +=                 55000000000000_u256; //adds to dust only but dust overflows +1 to balance

        //expect:           415000200010000; .0004 EOS, 15000200010000 dust
        check(b.balance.amount == 4, "");
        check(b.dust == 15000200010000, "");

        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b -=                             45_u256; //substracts from dust only
        b -=                100000000000000_u256; //subtracts strictly from balance
        b -=                120000000000000_u256; //subtracts from both dust and balance, causes underflow on dust thus -1 balance

        //expect:           195000200009955; .0001 EOS, 95000200009955 dust
        check(b.balance.amount == 1, "");
        check(b.dust == 95000200009955, "");
    }
    else if(test == "underflow1"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b -=                             45_u256;
        //should fail with underflow on dust causing an underflow of balance
    }
    else if(test == "underflow2"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b -=                100000000000000_u256;
        //should fail with underflow on balance
    }
    else if(test == "underflow3"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b +=                200000000000000_u256;
        b -=                300000000000000_u256;
        //should fail with underflow on balance
    }
    else if(test == "underflow4"_n) {
        balance_with_dust b;
        //                  ↱minimum EOS
        //              .123456789abcdefghi EEOS
        b +=                          50000_u256;
        b -=                      500000000_u256;
        //should fail with underflow on dust causing an underflow of balance
    }
    else if(test == "underflow5"_n) {
        balance_with_dust b;
        // do a decrement that would overflow an int64_t but not uint64_t (for balance)
        //    ↱int64t max       ↱minimum EOS
        //    9223372036854775807 (2^63)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b +=                              50000_u256;
        b -=  100000000000000000000000000000000_u256;
        //should fail with underflow
    }
    else if(test == "overflow1"_n) {
        balance_with_dust b;
        // increment a value that would overflow a int64_t, but not uint64_t
        //    ↱int64t max       ↱minimum EOS
        //    9223372036854775807 (2^63)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b += 1000000000000000000000000000000000_u256;
        //should fail with overflow
    }
    else if(test == "overflow2"_n) {
        balance_with_dust b;
        // increment a value that would overflow a max_asset, but not an int64_t
        //    ↱max_asset max    ↱minimum EOS
        //    4611686018427387903 (2^62)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b +=  500000000000000000000000000000000_u256;
        //should fail with overflow
    }
    else if(test == "overflow3"_n || test == "overflow4"_n || test == "overflow5"_n || test == "overflowa"_n || test == "overflowb"_n || test == "overflowc"_n) {
        balance_with_dust b;
        // start with a value that should be the absolute max allowed
        //    ↱max_asset max    ↱minimum EOS
        //    4611686018427387903 (2^62)-1
        //    543210987654321̣123456789abcdefghi EEOS
        b +=  461168601842738790399999999999999_u256;
        if(test == "overflow4"_n) {
            //add 1 to balance, should fail since it rolls balance over max_asset
            //                      ↱minimum EOS
            //                  .123456789abcdefghi EEOS
            b +=                    100000000000000_u256;
            //should fail with overflow
        }
        if(test == "overflow5"_n) {
            //add 1 to dust, causing a rollover making balance > max_asset
            //                      ↱minimum EOS
            //                  .123456789abcdefghi EEOS
            b +=                                  1_u256;
            //should fail with overflow
        }
        if(test == "overflowa"_n) {
            //add something huge
            //       ↱max_asset max    ↱minimum EOS
            //       4611686018427387903 (2^62)-1
            //       543210987654321̣123456789abcdefghi EEOS
            b +=  999461168601842738790399999999999999_u256;
            //should fail with overflow
        }
        if(test == "overflowb"_n) {
            // add max allowed to balance again; this should be a 2^62-1 + 2^62-1
            //    ↱max_asset max    ↱minimum EOS
            //    4611686018427387903 (2^62)-1
            //    543210987654321̣123456789abcdefghi EEOS
            b +=  461168601842738790300000000000000_u256;
            //should fail with overflow
        }
        if(test == "overflowc"_n) {
            // add max allowed to balance again; but also with max dust; should be a 2^62-1 + 2^62-1 + 1 on asset balance
            //    ↱max_asset max    ↱minimum EOS
            //    4611686018427387903 (2^62)-1
            //    543210987654321̣123456789abcdefghi EEOS
            b +=  461168601842738790399999999999999_u256;
            //should fail with overflow
        }
    }
    if(test == "overflowd"_n) {
        balance_with_dust b;
        //add something massive
        //            ↱max_asset max    ↱minimum EOS
        //            4611686018427387903 (2^62)-1
        //            543210987654321̣123456789abcdefghi EEOS
        b +=  99999999461168601842738790399999999999999_u256;
        //should fail with overflow
    }
}

}