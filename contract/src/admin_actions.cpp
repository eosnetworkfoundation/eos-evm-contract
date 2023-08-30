#include <eosio/system.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>

namespace evm_runtime {
[[eosio::action]] void evm_contract::rmgcstore(uint64_t id) {
    eosio::require_auth(get_self());
    gc_store_table gc(get_self(), get_self().value);
    auto itr = gc.find(id);
    eosio::check(itr != gc.end(), "gc row not found");
    gc.erase(*itr);
}

[[eosio::action]] void evm_contract::setkvstore(uint64_t account_id, const bytes& key, const std::optional<bytes>& value) {
    eosio::require_auth(get_self());
    eosio::check(key.size() == 32 && (!value.has_value() || value.value().size() == 32), "invalid key/value size");

    storage_table db(get_self(), account_id);
    auto inx = db.get_index<"by.key"_n>();
    auto itr = inx.find(make_key(key));

    if(value.has_value()) {
        if(itr == inx.end()) {
            db.emplace(get_self(), [&](auto& row){
                row.id = db.available_primary_key();
                row.key = key;
                row.value = value.value();
            });
        } else {
            db.modify(*itr, eosio::same_payer, [&](auto& row){
                row.value = value.value();
            });
        }
    } else {
        eosio::check(itr != inx.end(), "key not found");
        db.erase(*itr);
    }
}

[[eosio::action]] void evm_contract::rmaccount(uint64_t id) {
    eosio::require_auth(get_self());
    account_table accounts(get_self(), get_self().value);
    auto itr = accounts.find(id);
    eosio::check(itr != accounts.end(), "account not found");

    if (itr->code_id) {
        account_code_table codes(get_self(), get_self().value);
        const auto& itrc = codes.get(itr->code_id.value(), "code not found");
        if(itrc.ref_count-1) {
            codes.modify(itrc, eosio::same_payer, [&](auto& row){
                row.ref_count--;
            });
        } else {
            codes.erase(itrc);
        }
    }

    gc_store_table gc(get_self(), get_self().value);
    gc.emplace(get_self(), [&](auto& row){
        row.id = gc.available_primary_key();
        row.storage_id = itr->id;
    });

    accounts.erase(*itr);
}

[[eosio::action]] void evm_contract::addevmbal(uint64_t id, const bytes& delta, bool subtract) {
    eosio::require_auth(get_self());
    account_table accounts(get_self(), get_self().value);
    auto itr = accounts.find(id);
    eosio::check(itr != accounts.end(), "account not found");

    inevm_singleton inevm(get_self(), get_self().value);
    auto d = to_uint256(delta);

    intx::result_with_carry<intx::uint256> res;
    if(subtract) {
        inevm.set(inevm.get()-=d, eosio::same_payer);
        res = intx::subc(to_uint256(itr->balance), d);
        eosio::check(!res.carry, "underflow detected");
    } else {
        res = intx::addc(to_uint256(itr->balance), d);
        eosio::check(!res.carry, "overflow detected");
        inevm.set(inevm.get()+=d, eosio::same_payer);
    }

    accounts.modify(*itr, eosio::same_payer, [&](auto& row){
        row.balance = to_bytes(res.value);
    });
}

[[eosio::action]] void evm_contract::addopenbal(name account, const asset& delta) {
    eosio::require_auth(get_self());
    balances open_balances(get_self(), get_self().value);
    auto itr = open_balances.find(account.value);
    eosio::check(itr != open_balances.end(), "account not found");

    auto res = itr->balance.balance + delta;
    eosio::check(res.amount >= 0, "negative final balance");

    open_balances.modify(*itr, eosio::same_payer, [&](auto& row){
        row.balance.balance = res;
    });
}

[[eosio::action]] void evm_contract::freezeaccnt(uint64_t id, bool value) {
    eosio::require_auth(get_self());
    account_table accounts(get_self(), get_self().value);
    auto itr = accounts.find(id);
    eosio::check(itr != accounts.end(), "account not found");

    accounts.modify(*itr, eosio::same_payer, [&](auto& row){
        if(value) {
            row.set_flag(account::flag::frozen);
        } else {
            row.clear_flag(account::flag::frozen);
        }
    });
}

}
