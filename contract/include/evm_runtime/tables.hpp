#pragma once

#include <eosio/eosio.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <evm_runtime/types.hpp>
#include <silkworm/common/base.hpp>
namespace evm_runtime {

using namespace eosio;
struct [[eosio::table]] [[eosio::contract("evm_contract")]] account {
    uint64_t    id;
    bytes       eth_address;
    uint64_t    nonce;
    bytes       balance;
    std::optional<uint64_t>       code_id;

    uint64_t primary_key()const { return id; }

    checksum256 by_eth_address()const { 
        return make_key(eth_address);
    }

    uint256be get_balance()const {
        uint256be res;
        std::copy(balance.begin(), balance.end(), res.bytes);
        return res;
    }

    EOSLIB_SERIALIZE(account, (id)(eth_address)(nonce)(balance)(code_id));
};

typedef multi_index< "account"_n, account,
    indexed_by<"by.address"_n, const_mem_fun<account, checksum256, &account::by_eth_address>>
> account_table;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] account_code {
    uint64_t    id;
    uint32_t    ref_count;
    bytes       code;
    bytes       code_hash;

    uint64_t primary_key()const { return id; }

    checksum256 by_code_hash()const { 
        return make_key(code_hash);
    }

    bytes32 get_code_hash()const {
        return to_bytes32(code_hash);
    }

    EOSLIB_SERIALIZE(account_code, (id)(ref_count)(code)(code_hash));
};

typedef multi_index< "accountcode"_n, account_code,
    indexed_by<"by.codehash"_n, const_mem_fun<account_code, checksum256, &account_code::by_code_hash>>
> account_code_table;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] storage {
    uint64_t id;
    bytes    key;
    bytes    value;

    uint64_t primary_key()const { return id; }

    checksum256 by_key()const { 
        return make_key(key);
    }

    EOSLIB_SERIALIZE(storage, (id)(key)(value));
};

typedef multi_index< "storage"_n, storage,
    indexed_by<"by.key"_n, const_mem_fun<storage, checksum256, &storage::by_key>> 
> storage_table;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] gcstore {
    uint64_t id;
    uint64_t storage_id;

    uint64_t primary_key()const { return id; }

    EOSLIB_SERIALIZE(gcstore, (id)(storage_id));
};

typedef multi_index< "gcstore"_n, gcstore> gc_store_table;

struct [[eosio::table("inevm")]] [[eosio::contract("evm_contract")]] balance_with_dust {
    asset balance = asset(0, token_symbol);
    uint64_t dust = 0;

    bool operator==(const balance_with_dust& o) const {
        return balance == o.balance && dust == o.dust;
    }
    bool operator!=(const balance_with_dust& o) const {
        return !(*this == o);
    }

    balance_with_dust& operator+=(const intx::uint256& amount) {
        const intx::div_result<intx::uint256> div_result = udivrem(amount, minimum_natively_representable);

        //asset::max_amount is conservative at 2^62-1, this means two max_amounts of (2^62-1)+(2^62-1) cannot
        // overflow an int64_t which can represent up to 2^63-1. In other words, asset::max_amount+asset::max_amount
        // are guaranteed greater than asset::max_amount without need to worry about int64_t overflow. Even more,
        // asset::max_amount+asset::max_amount+1 is guaranteed greater than asset::max_amount without need to worry
        // about int64_t overflow. The latter property ensures that if the existing value is max_amount and max_amount
        // is added with a dust roll over, an int64_t rollover still does not occur on the balance.
        //This means that we just need to check that whatever we're adding is no more than 2^62-1 (max_amount), and that
        // the current value is no more than 2^62-1 (max_amount), and adding them together will not overflow.
        check(div_result.quot <= asset::max_amount, "accumulation overflow");
        check(balance.amount <= asset::max_amount, "accumulation overflow");

        const int64_t base_amount = div_result.quot[0];
        balance.amount += base_amount;
        dust += div_result.rem[0];

        if(dust >= min_asset) {
            balance.amount++;
            dust -= min_asset;
        }

        check(balance.amount <= asset::max_amount, "accumulation overflow");

        return *this;
    }

    balance_with_dust& operator-=(const intx::uint256& amount) {
        const intx::div_result<intx::uint256> div_result = udivrem(amount, minimum_natively_representable);

        check(div_result.quot <= balance.amount, "decrementing more than available");

        balance.amount -= div_result.quot[0];
        dust -= div_result.rem[0];

        if(dust & (UINT64_C(1) << 63)) {
            balance.amount--;
            dust += min_asset;
            check(balance.amount >= 0, "decrementing more than available");
        }

        return *this;
    }

    static constexpr uint64_t min_asset = minimum_natively_representable[0];

    EOSLIB_SERIALIZE(balance_with_dust, (balance)(dust));
};

struct [[eosio::table]] [[eosio::contract("evm_contract")]] balance {
    name              owner;
    balance_with_dust balance;

    uint64_t primary_key() const { return owner.value; }

    EOSLIB_SERIALIZE(struct balance, (owner)(balance));
};

typedef eosio::multi_index<"balances"_n, balance> balances;

typedef eosio::singleton<"inevm"_n, balance_with_dust> inevm_singleton;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] nextnonce {
    name     owner;
    uint64_t next_nonce = 0;

    uint64_t primary_key() const { return owner.value; }

    EOSLIB_SERIALIZE(nextnonce, (owner)(next_nonce));
};

typedef eosio::multi_index<"nextnonces"_n, nextnonce> nextnonces;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] allowed_egress_account {
    name account;

    uint64_t primary_key() const { return account.value; }

    EOSLIB_SERIALIZE(allowed_egress_account, (account));
};

typedef eosio::multi_index<"egresslist"_n, allowed_egress_account> egresslist;

} //namespace evm_runtime