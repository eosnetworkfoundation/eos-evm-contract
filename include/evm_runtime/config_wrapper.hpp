#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>
#include <evm_runtime/tables.hpp>
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

    uint64_t get_gas_price()const;

    uint32_t get_miner_cut()const;
    void set_miner_cut(uint32_t miner_cut);

    uint32_t get_status()const;
    void set_status(uint32_t status);

    uint64_t get_evm_version()const;
    uint64_t get_evm_version_and_maybe_promote();
    void set_evm_version(uint64_t new_version);

    void set_fee_parameters(const fee_parameters& fee_params,
                            bool allow_any_to_be_unspecified);

    void update_gas_params(eosio::asset ram_price_mb, uint64_t minimum_gas_price);
    void update_gas_params2(std::optional<uint64_t> gas_txnewaccount, std::optional<uint64_t> gas_newaccount, std::optional<uint64_t> gas_txcreate, std::optional<uint64_t> gas_codedeposit, std::optional<uint64_t> gas_sset, std::optional<uint64_t> minimum_gas_price);

    std::pair<const consensus_parameter_data_type &, bool> get_consensus_param_and_maybe_promote();

private:
    bool is_dirty()const;
    void set_dirty();
    void clear_dirty();

    eosio::time_point get_current_time()const;

    bool _dirty  = false;
    bool _exists = false;
    config _cached_config;

    eosio::name _self;
    eosio::singleton<"config"_n, config> _config;
    mutable std::optional<eosio::time_point> current_time_point;
};

} //namespace evm_runtime