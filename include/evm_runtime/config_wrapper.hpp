#pragma once

#include <evm_runtime/tables.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>
namespace evm_runtime {

struct fee_parameters;
struct config_wrapper {

    config_wrapper(eosio::name self);
    ~config_wrapper();

    void flush();
    bool exists();

    eosio::unsigned_int get_version()const;
    void set_version(const eosio::unsigned_int version);

    uint64_t get_chainid()const;
    void set_chainid(uint64_t chainid);

    const eosio::time_point_sec& get_genesis_time()const;
    void set_genesis_time(eosio::time_point_sec genesis_time);

    const eosio::asset& get_ingress_bridge_fee()const;
    void set_ingress_bridge_fee(const eosio::asset& ingress_bridge_fee);

    gas_prices_type get_gas_prices()const;
    void set_gas_prices(const gas_prices_type& price);

    uint64_t get_gas_price()const;
    void set_gas_price(uint64_t gas_price);

    template <typename Q, typename V>
    void enqueue(const V& new_value);
    void enqueue_gas_price(uint64_t gas_price);
    void enqueue_gas_prices(const gas_prices_type& prices);

    template <typename Q, typename Func>
    void process_queue(Func&& update_func);
    void process_price_queue();

    uint32_t get_miner_cut()const;
    void set_miner_cut(uint32_t miner_cut);

    uint32_t get_status()const;
    void set_status(uint32_t status);

    uint64_t get_evm_version()const;
    uint64_t get_evm_version_and_maybe_promote();
    void set_evm_version(uint64_t new_version);

    void set_fee_parameters(const fee_parameters& fee_params,
                            bool allow_any_to_be_unspecified);

    void update_consensus_parameters(eosio::asset ram_price_mb, uint64_t gas_price);
    void update_consensus_parameters2(std::optional<uint64_t> gas_txnewaccount, std::optional<uint64_t> gas_newaccount, std::optional<uint64_t> gas_txcreate, std::optional<uint64_t> gas_codedeposit, std::optional<uint64_t> gas_sset);

    const consensus_parameter_data_type& get_consensus_param();
    std::pair<const consensus_parameter_data_type&, bool> get_consensus_param_and_maybe_promote();

    void set_token_contract(eosio::name token_contract); // only set during init
    eosio::name get_token_contract() const;
    eosio::symbol get_token_symbol() const;
    uint64_t get_minimum_natively_representable() const;

    void set_ingress_gas_limit(uint64_t gas_limit);
    uint64_t get_ingress_gas_limit() const;

    void swapgastoken(name new_token_contract, symbol new_symbol);

private:
    void set_queue_front_block(uint32_t block_num);
    
    bool is_dirty()const;
    void set_dirty();
    void clear_dirty();

    uint64_t is_same_as_current_price(uint64_t new_value) {
        return _cached_config.gas_price == new_value;
    };

    bool is_same_as_current_price(const gas_prices_type& new_value) {
        if(new_value.overhead_price.has_value() && _cached_config.gas_prices.value().overhead_price != new_value.overhead_price) {
            return false;
        }
        if(new_value.storage_price.has_value() && _cached_config.gas_prices.value().storage_price != new_value.storage_price) {
            return false;
        }
        return true;
    };

    eosio::time_point get_current_time()const;
    bool check_gas_overflow(uint64_t gas_txcreate, uint64_t gas_codedeposit) const; // return true if pass

    bool _dirty  = false;
    bool _exists = false;
    config _cached_config;

    eosio::name _self;
    eosio::singleton<"config"_n, config> _config;
    mutable std::optional<eosio::time_point> current_time_point;
};

} //namespace evm_runtime