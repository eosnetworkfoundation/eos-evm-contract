#pragma once

#include <eosio/eosio.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <evm_runtime/types.hpp>

namespace evm_runtime {

using namespace eosio;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] account {
    uint64_t    id;
    bytes       eth_address;
    uint64_t    nonce;
    bytes       balance;
    bytes       code;
    bytes       code_hash;

    uint64_t primary_key()const { return id; }

    checksum256 by_eth_address()const { 
        return make_key(eth_address);
    }

    checksum256 by_code_hash()const { 
        return make_key(code_hash);
    }

    uint256be get_balance()const {
        uint256be res;
        std::copy(balance.begin(), balance.end(), res.bytes);
        return res;
    }

    bytes32 get_code_hash()const {
        bytes32 res;
        std::copy(code_hash.begin(), code_hash.end(), res.bytes);
        return res;
    }

    EOSLIB_SERIALIZE(account, (id)(eth_address)(nonce)(balance)(code)(code_hash));
};

typedef multi_index< "account"_n, account,
    indexed_by<"by.address"_n, const_mem_fun<account, checksum256, &account::by_eth_address>>,
    indexed_by<"by.codehash"_n, const_mem_fun<account, checksum256, &account::by_code_hash>>
> account_table;

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

struct balance_with_dust {
    asset balance = asset(0, token_symbol);
    uint64_t dust = 0;

    bool operator==(const balance_with_dust& o) const {
        return balance == o.balance && dust == o.dust;
    }
    bool operator!=(const balance_with_dust& o) const {
        return !(*this == o);
    }

    void accumulate(const intx::uint256& amount) {
        //asset::max_amount is conservative at 2^62-1, this means two amounts of (2^62-1)+(2^62-1) cannot
        // overflow an int64_t which can represent up to 2^63-1. In other words, asset::max_amount+asset::max_amount
        // are guaranteed greater than asset::max_amount without need to worry about int64_t overflow
        check(amount/min_asset_bn <= asset::max_amount, "accumulation overflow");

        const int64_t base_amount = (amount/min_asset_bn)[0];
        check(balance.amount + base_amount < asset::max_amount, "accumulation overflow");
        balance.amount += base_amount;
        dust += (amount%min_asset_bn)[0];

        if(dust > min_asset) {
            balance.amount++;
            dust -= min_asset;
        }
    }

    void decrement(const intx::uint256& amount) {
        check(amount/min_asset_bn <= balance.amount, "decrementing more than available");
        balance.amount -= (amount/min_asset_bn)[0];
        dust -= (amount%min_asset_bn)[0];

        if(dust & (UINT64_C(1) << 63)) {
            balance.amount--;
            dust += min_asset;
            check(balance.amount >= 0, "decrementing more than available");
        }
    }

    static constexpr intx::uint256 min_asset_bn = intx::exp(10_u256, intx::uint256(evm_precision - token_symbol.precision()));
    static constexpr uint64_t min_asset = min_asset_bn[0];

    EOSLIB_SERIALIZE(balance_with_dust, (balance)(dust));
};

struct [[eosio::table]] [[eosio::contract("evm_contract")]] balance {
    name              owner;
    balance_with_dust balance;

    uint64_t primary_key() const { return owner.value; }

    EOSLIB_SERIALIZE(struct balance, (owner)(balance));
};

typedef eosio::multi_index<"balances"_n, balance> balances;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] stats {
    balance_with_dust in_evm;

    EOSLIB_SERIALIZE(stats, (in_evm));
};

typedef eosio::singleton<"stats"_n, stats> stats_singleton;

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