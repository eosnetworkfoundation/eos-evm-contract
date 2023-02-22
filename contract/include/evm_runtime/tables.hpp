#pragma once

#include <eosio/eosio.hpp>
#include <eosio/fixed_bytes.hpp>
#include <evm_runtime/types.hpp>

namespace evm_runtime {

using namespace eosio;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] account {
    uint64_t    id;
    bytes       eth_address;
    uint64_t    nonce;
    bytes       balance;
    bytes       code_hash;

    uint64_t primary_key()const { return id; }

    checksum256 by_eth_address()const { 
        return make_key(eth_address);
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

    EOSLIB_SERIALIZE(account, (id)(eth_address)(nonce)(balance)(code_hash));
};

typedef multi_index< "account"_n, account,
    indexed_by<"by.address"_n, const_mem_fun<account, checksum256, &account::by_eth_address>>
> account_table;

struct [[eosio::table]] [[eosio::contract("evm_contract")]] codestore {
    uint64_t    id;
    bytes       code;
    bytes       code_hash;

    uint64_t primary_key()const { return id; }

    checksum256 by_code_hash()const { 
        return make_key(code_hash);
    }

    bytes32 get_code_hash()const {
        bytes32 res;
        std::copy(code_hash.begin(), code_hash.end(), res.bytes);
        return res;
    }

    EOSLIB_SERIALIZE(codestore, (id)(code)(code_hash));
};

typedef multi_index< "codestore"_n, codestore,
    indexed_by<"by.codehash"_n, const_mem_fun<codestore, checksum256, &codestore::by_code_hash>>
> codestore_table;

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

} //namespace evm_runtime