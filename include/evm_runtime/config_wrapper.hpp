#pragma once
#include <eosio/eosio.hpp>
#include <evm_runtime/tables.hpp>

namespace evm_runtime {

using eosio::check;

struct config_wrapper {

    config_wrapper(eosio::name self) : _self(self), _config(self, self.value) {
        _exists = _config.exists();
        if(_exists) {
            _cached_config = _config.get();
        }
    }

    ~config_wrapper() {
        flush();
    }

    void flush() {
        if( _evm_version_promote_required ) {
            promote_pending_evm_version();
            set_dirty();
        }
        if(!is_dirty()) {
            return;
        }
        _config.set(_cached_config, _self);
        clear_dirty();
        _exists = true;
    }

    bool exists() {
        return _exists;
    }

    inline eosio::unsigned_int get_version()const { return _cached_config.version; }
    void set_version(const eosio::unsigned_int version) { _cached_config.version = version; set_dirty(); }

    inline uint64_t get_chainid()const { return _cached_config.chainid; }
    void set_chainid(uint64_t chainid) { _cached_config.chainid = chainid; set_dirty(); }

    inline const eosio::time_point_sec& get_genesis_time()const { return _cached_config.genesis_time; }
    void set_genesis_time(eosio::time_point_sec genesis_time) { _cached_config.genesis_time = genesis_time; set_dirty(); }

    inline const eosio::asset& get_ingress_bridge_fee()const { return _cached_config.ingress_bridge_fee; }
    void set_ingress_bridge_fee(const eosio::asset& ingress_bridge_fee) { _cached_config.ingress_bridge_fee = ingress_bridge_fee; set_dirty(); }

    inline uint64_t get_gas_price()const { return _cached_config.gas_price; }
    void set_gas_price(uint64_t gas_price) { _cached_config.gas_price = gas_price; set_dirty(); }

    inline uint32_t get_miner_cut()const { return _cached_config.miner_cut; }
    void set_miner_cut(uint32_t miner_cut) { _cached_config.miner_cut = miner_cut; set_dirty(); }

    inline uint32_t get_status()const { return _cached_config.status; }
    void set_status(uint32_t status) { _cached_config.status = status; set_dirty(); }

    inline uint64_t get_evm_version()const {
        uint64_t current_version = 0;
        _evm_version_promote_required = false;
        if(_cached_config.evm_version.has_value()) {
            std::tie(current_version, _evm_version_promote_required) = _cached_config.evm_version->get_version(_cached_config.genesis_time, eosio::current_time_point());
        }
        return current_version;
    }

    void set_evm_version(uint64_t new_version) {
        eosio::check(new_version <= MAX_EOSEVM_SUPPORTED_VERSION, "Unsupported version");
        auto current_version = get_evm_version();
        eosio::check(new_version > current_version, "new version must be greater than the active one");
        if( _evm_version_promote_required ) {
            promote_pending_evm_version();
        }
        auto current_time = eosio::current_time_point();
        _cached_config.evm_version.emplace(evm_version_type{evm_version_type::pending{new_version, current_time}, current_version});
        set_dirty();
    }

    void promote_pending_evm_version() {
        eosio::check(_cached_config.evm_version.has_value(), "no evm_version");
        _cached_config.evm_version->promote_pending();
        _evm_version_promote_required = false;
    }

    void set_fee_parameters(const fee_parameters& fee_params,
                            bool allow_any_to_be_unspecified)
    {
        if (fee_params.gas_price.has_value()) {
            _cached_config.gas_price = *fee_params.gas_price;
        } else {
            check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing gas_price");
        }

        if (fee_params.miner_cut.has_value()) {
            check(*fee_params.miner_cut <= hundred_percent, "miner_cut cannot exceed 100,000 (100%)");

            _cached_config.miner_cut = *fee_params.miner_cut;
        } else {
            check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing miner_cut");
        }

        if (fee_params.ingress_bridge_fee.has_value()) {
            check(fee_params.ingress_bridge_fee->symbol == token_symbol, "unexpected bridge symbol");
            check(fee_params.ingress_bridge_fee->amount >= 0, "ingress bridge fee cannot be negative");

            _cached_config.ingress_bridge_fee = *fee_params.ingress_bridge_fee;
        }

        set_dirty();
    }

private:
    inline bool is_dirty()const { return _dirty; }
    inline void set_dirty() { _dirty=true; }
    inline void clear_dirty() { _dirty=false; }

    bool _dirty  = false;
    bool _exists = false;
    mutable bool _evm_version_promote_required = false;
    config _cached_config;

    eosio::name _self;
    eosio::singleton<"config"_n, config> _config;
};

} //namespace evm_runtime