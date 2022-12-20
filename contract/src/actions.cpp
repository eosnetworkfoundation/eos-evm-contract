#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/processor.hpp>
#include <evm_runtime/state.hpp>
#include <evm_runtime/engine.hpp>
#include <evm_runtime/intrinsics.hpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/engine.hpp>
#endif

#ifdef WITH_LOGTIME
#define LOGTIME(MSG) eosio::internal_use_do_not_use::logtime(MSG)
#else
#define LOGTIME(MSG)
#endif

namespace evm_runtime {

using namespace silkworm;

void evm_contract::init(const uint64_t chainid, const symbol& native_token_symbol) {
    eosio::require_auth(get_self());

    check( !_config.exists(), "contract already initialized" );
    check( !!lookup_known_chain(chainid), "unknown chainid" );

    _config.set({
        .version = 0,
        .chainid = chainid,
        .genesis_time = current_time_point()
    }, get_self());

    stats statstable( get_self(), native_token_symbol.code().raw() );

    statstable.emplace( get_self(), [&]( currency_stats& s ) {
       s.supply.symbol = native_token_symbol;
       s.max_supply    = eosio::asset(eosio::asset::max_amount, native_token_symbol);
       s.issuer        = get_self();
    });
}

void evm_contract::pushtx( eosio::name ram_payer, const bytes& rlptx ) {
    assert_inited();

    LOGTIME("EVM START");
    eosio::require_auth(ram_payer);

    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(_config.get().chainid);
    check( found_chain_config.has_value(), "failed to find expected chain config" );

    Block block;
    block.header.difficulty  = 0;
    block.header.gas_limit   = INT64_MAX;
    block.header.number      = eosio::internal_use_do_not_use::get_block_num();
    block.header.timestamp   = eosio::current_time_point().sec_since_epoch();

    Transaction tx;
    ByteView bv{(const uint8_t*)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");
    LOGTIME("EVM TX DECODE");

    tx.from.reset();
    tx.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");
    LOGTIME("EVM RECOVER SENDER");

    evm_runtime::engine engine;
    evm_runtime::state state{get_self(), ram_payer};
    evm_runtime::ExecutionProcessor ep{block, engine, state, *found_chain_config->second};

    Receipt receipt;
    ep.execute_transaction(tx, receipt);
    
    LOGTIME("EVM EXECUTE");
}

void evm_contract::issue( const name& to, const asset& quantity, const std::string& memo ) {
    assert_inited();

    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "requested token symbol to issue does not match symbol from init action" );
    const auto& st = *existing;
    check( to == st.issuer, "tokens can only be issued to issuer account" );

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    open( st.issuer, quantity.symbol, st.issuer );
    add_balance( st.issuer, quantity );
}

void evm_contract::retire( const asset& quantity, const std::string& memo ) {
    assert_inited();

    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void evm_contract::open( const name& owner, const symbol& symbol, const name& ram_payer ) {
    assert_inited();

    require_auth( ram_payer );

    check( is_account( owner ), "owner account does not exist" );

    auto sym_code_raw = symbol.code().raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
    check( st.supply.symbol == symbol, "symbol precision mismatch" );

    accounts acnts( get_self(), owner.value );
    auto it = acnts.find( sym_code_raw );
    if( it == acnts.end() ) {
        acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = asset{0, symbol};
        });
    }
}

void evm_contract::close( const name& owner, const symbol& symbol ) {
    assert_inited();

    require_auth( owner );

    accounts acnts( get_self(), owner.value );
    auto it = acnts.find( symbol.code().raw() );
    check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
    check( it->balance.amount == 0 && it->dust == 0, "Cannot close because the balance is not zero." );
    acnts.erase( it );
}

void evm_contract::transfer( const name& from, const name& to, const asset& quantity, const std::string& memo ) {
    assert_inited();

    check( from != to, "cannot transfer to self" );
    require_auth( from );

    check( is_account( to ), "to account does not exist");

    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    sub_balance( from, quantity );
    add_balance( to, quantity );
}

void evm_contract::sub_balance( const name& owner, const asset& value ) {
    accounts from_acnts( get_self(), owner.value );

    const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
    check( from.balance.amount >= value.amount, "overdrawn balance" );

    from_acnts.modify( from, eosio::same_payer, [&]( auto& a ) {
        a.balance -= value;
    });
}

void evm_contract::add_balance( const name& owner, const asset& value ) {
    accounts to_acnts( get_self(), owner.value );

    const auto& to = to_acnts.get( value.symbol.code().raw(), "account transfering to must already be open" );

    to_acnts.modify( to, eosio::same_payer, [&]( auto& a ) {
        a.balance += value;
    });
}

#ifdef WITH_TEST_ACTIONS
ACTION evm_contract::testtx( const bytes& rlptx, const evm_runtime::test::block_info& bi ) {
    assert_inited();

    eosio::require_auth(get_self());

    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(_config.get().chainid);
    check( found_chain_config.has_value(), "failed to find expected chain config" );
    
    Block block;
    block.header = bi.get_block_header();

    Transaction tx;
    ByteView bv{(const uint8_t *)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");

    tx.from.reset();
    tx.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");

    evm_runtime::test::engine engine;
    evm_runtime::state state{get_self(), get_self()};
    evm_runtime::ExecutionProcessor ep{block, engine, state, *found_chain_config->second};

    Receipt receipt;
    ep.execute_transaction(tx, receipt);
}

ACTION evm_contract::dumpstorage(const bytes& addy) {
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

ACTION evm_contract::dumpall() {
    assert_inited();

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
    assert_inited();

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
    assert_inited();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto bvcode = ByteView{(const uint8_t *)code.data(), code.size()};
    state.update_account_code(to_address(address), incarnation, to_bytes32(code_hash), bvcode);
}

ACTION evm_contract::updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current) {
    assert_inited();

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
    assert_inited();

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

ACTION evm_contract::setbal(const bytes& addy, const bytes& bal) {
    assert_inited();

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
