#pragma once
#include <evm_runtime/config_wrapper.hpp>
#include <evm_runtime/tables.hpp>

namespace evm_runtime {

config_wrapper::config_wrapper(eosio::name self) : _self(self), _config(self, self.value) {
    _exists = _config.exists();
    if(_exists) {
        _cached_config = _config.get();
    }
}

config_wrapper::~config_wrapper() {
    flush();
}

void config_wrapper::flush() {
    if(!is_dirty()) {
        return;
    }
    _config.set(_cached_config, _self);
    clear_dirty();
    _exists = true;
}

bool config_wrapper::exists() {
    return _exists;
}

eosio::unsigned_int config_wrapper::get_version()const { 
    return _cached_config.version;
}

void config_wrapper::set_version(const eosio::unsigned_int version) {
    _cached_config.version = version;
    set_dirty();
}

uint64_t config_wrapper::get_chainid()const {
    return _cached_config.chainid;
}

void config_wrapper::set_chainid(uint64_t chainid) {
    _cached_config.chainid = chainid;
    set_dirty();
}

const eosio::time_point_sec& config_wrapper::get_genesis_time()const {
    return _cached_config.genesis_time;
}

void config_wrapper::set_genesis_time(eosio::time_point_sec genesis_time) {
    _cached_config.genesis_time = genesis_time;
    set_dirty();
}

const eosio::asset& config_wrapper::get_ingress_bridge_fee()const {
    return _cached_config.ingress_bridge_fee;
}

void config_wrapper::set_ingress_bridge_fee(const eosio::asset& ingress_bridge_fee) {
    _cached_config.ingress_bridge_fee = ingress_bridge_fee;
    set_dirty();
}

uint64_t config_wrapper::get_gas_price()const {
    uint64_t gas_price = _cached_config.gas_price;
    if (_cached_config.consensus_parameter.has_value() && 
        _cached_config.consensus_parameter->is_pending_active(_cached_config.genesis_time, get_current_time())) {
        std::visit([&](const auto &v) {
            if (v.minimum_gas_price) gas_price = v.minimum_gas_price;
        }, *(_cached_config.consensus_parameter->pending));
    }
    return gas_price;
}

uint32_t config_wrapper::get_miner_cut()const {
    return _cached_config.miner_cut;
}

void config_wrapper::set_miner_cut(uint32_t miner_cut) {
    eosio::check(miner_cut <= ninety_percent, "miner_cut must <= 90%");
    _cached_config.miner_cut = miner_cut;
    set_dirty();
}

uint32_t config_wrapper::get_status()const {
    return _cached_config.status;
}

void config_wrapper::set_status(uint32_t status) {
    _cached_config.status = status;
    set_dirty();
}

uint64_t config_wrapper::get_evm_version()const {
    uint64_t current_version = 0;
    if(_cached_config.evm_version.has_value()) {
        current_version = _cached_config.evm_version->get_version(_cached_config.genesis_time, get_current_time());
    }
    return current_version;
}

uint64_t config_wrapper::get_evm_version_and_maybe_promote() {
    uint64_t current_version = 0;
    bool promoted = false;
    if(_cached_config.evm_version.has_value()) {
        std::tie(current_version, promoted) = _cached_config.evm_version->get_version_and_maybe_promote(_cached_config.genesis_time, get_current_time());
    }
    if(promoted) set_dirty();
    return current_version;
}

void config_wrapper::set_evm_version(uint64_t new_version) {
    eosio::check(new_version <= eosevm::max_eos_evm_version, "Unsupported version");
    auto current_version = get_evm_version_and_maybe_promote();
    eosio::check(new_version > current_version, "new version must be greater than the active one");
    _cached_config.evm_version.emplace(evm_version_type{evm_version_type::pending{new_version, get_current_time()}, current_version});
    set_dirty();
}

void config_wrapper::set_fee_parameters(const fee_parameters& fee_params,
                        bool allow_any_to_be_unspecified)
{
    if (fee_params.gas_price.has_value()) {
        eosio::check(*fee_params.gas_price >= 1000000000ull, "gas_price must >= 1Gwei");
        if (_cached_config.evm_version.has_value() && _cached_config.evm_version->cached_version >= 1) {
            // activate in the next evm block
            this->update_gas_params2(std::optional<uint64_t>(), /* gas_txnewaccount */
                                     std::optional<uint64_t>(), /* gas_newaccount */
                                     std::optional<uint64_t>(), /* gas_txcreate */
                                     std::optional<uint64_t>(), /* gas_codedeposit */
                                     std::optional<uint64_t>(), /* gas_sset */
                                     *fee_params.gas_price      /* minimum_gas_price */
            );
        } else {
            _cached_config.gas_price = *fee_params.gas_price;
        }
    } else {
        eosio::check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing gas_price");
    }

    if (fee_params.miner_cut.has_value()) {
        eosio::check(*fee_params.miner_cut <= ninety_percent, "miner_cut must <= 90%");
        _cached_config.miner_cut = *fee_params.miner_cut;
    } else {
        eosio::check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing miner_cut");
    }

    if (fee_params.ingress_bridge_fee.has_value()) {
        eosio::check(fee_params.ingress_bridge_fee->symbol == token_symbol, "unexpected bridge symbol");
        eosio::check(fee_params.ingress_bridge_fee->amount >= 0, "ingress bridge fee cannot be negative");

        _cached_config.ingress_bridge_fee = *fee_params.ingress_bridge_fee;
    }

    set_dirty();
}

void config_wrapper::update_gas_params(eosio::asset ram_price_mb, uint64_t minimum_gas_price) {

    eosio::check(ram_price_mb.symbol == token_symbol, "invalid price symbol");
    eosio::check(minimum_gas_price >= 1000000000ull, "gas_price must >= 1Gwei");

    double gas_per_byte_f = (ram_price_mb.amount / 10000.0 * 1e18 / (1024.0 * 1024.0)) / 
        (minimum_gas_price * (double)(100000 - _cached_config.miner_cut) / 100000.0);

    constexpr uint64_t account_bytes = 347;
    constexpr uint64_t contract_fixed_bytes = 606;
    constexpr uint64_t storage_slot_bytes = 346;

    eosio::check(gas_per_byte_f >= 0.0, "gas_per_byte must >= 0");
    eosio::check(gas_per_byte_f * contract_fixed_bytes < (double)(0x7ffffffffffull), "gas_per_byte too big");

    uint64_t gas_per_byte = (uint64_t)(gas_per_byte_f + 1.0);

    this->update_gas_params2(account_bytes * gas_per_byte, /* gas_txnewaccount */
                             account_bytes * gas_per_byte, /* gas_newaccount */
                             contract_fixed_bytes * gas_per_byte, /*gas_txcreate*/
                             gas_per_byte,/*gas_codedeposit*/
                             100 + storage_slot_bytes * gas_per_byte,/*gas_sset*/
                             minimum_gas_price /*minimum_gas_price*/
    );
}

void config_wrapper::update_gas_params2(std::optional<uint64_t> gas_txnewaccount, std::optional<uint64_t> gas_newaccount, std::optional<uint64_t> gas_txcreate, std::optional<uint64_t> gas_codedeposit, std::optional<uint64_t> gas_sset, std::optional<uint64_t> minimum_gas_price)
{
    // for simplicity, ensure last (cached) evm_version >= 1, not touching promote logic
    eosio::check(_cached_config.evm_version.has_value() && _cached_config.evm_version->cached_version >= 1,
        "evm_version must >= 1");

    // for simplcity, wait for at least 1 trx to trigger the creation of _cached_config.consensus_parameter
    eosio::check(_cached_config.consensus_parameter.has_value(), "consensus_parameter must exist");

    if (minimum_gas_price.has_value()) {
        eosio::check(*minimum_gas_price >= 1000000000ull, "gas_price must >= 1Gwei");
    }

    _cached_config.consensus_parameter->update_consensus_param([&](auto & v) {
        if (gas_txnewaccount.has_value()) v.gas_parameter.gas_txnewaccount = *gas_txnewaccount;
        if (gas_newaccount.has_value()) v.gas_parameter.gas_newaccount = *gas_newaccount;
        if (gas_txcreate.has_value()) v.gas_parameter.gas_txcreate = *gas_txcreate;
        if (gas_codedeposit.has_value()) v.gas_parameter.gas_codedeposit = *gas_codedeposit;
        if (gas_sset.has_value()) v.gas_parameter.gas_sset = *gas_sset;
        if (minimum_gas_price.has_value()) {
            v.minimum_gas_price = *minimum_gas_price;
        } else if (v.minimum_gas_price == 0) {
            v.minimum_gas_price = _cached_config.gas_price;
        }
    }, get_current_time());

    set_dirty();
}

std::pair<const consensus_parameter_data_type &, bool> config_wrapper::get_consensus_param_and_maybe_promote() {
    if (!_cached_config.consensus_parameter.has_value()) {
        _cached_config.consensus_parameter = consensus_parameter_type();
        set_dirty();
        return std::pair<const consensus_parameter_data_type &, bool>(_cached_config.consensus_parameter->current, false);
    }

    auto pair = _cached_config.consensus_parameter->get_consensus_param_and_maybe_promote(_cached_config.genesis_time, get_current_time());

    if (pair.second) { // update
        // populate minimum_gas_price to config only if minimum_gas_price > 0
        uint64_t minimum_gas_price = 0;
        std::visit([&](const auto &v) {
            minimum_gas_price = v.minimum_gas_price;
        }, pair.first);
        if (minimum_gas_price) _cached_config.gas_price = minimum_gas_price;
        set_dirty();
    }

    return pair;
}

bool config_wrapper::is_dirty()const {
    return _dirty;
}

void config_wrapper::set_dirty() {
    _dirty=true;
}

void config_wrapper::clear_dirty() {
    _dirty=false;
}

time_point config_wrapper::get_current_time()const {
    if(!current_time_point.has_value()) {
        current_time_point = eosio::current_time_point();
    }
    return current_time_point.value();
}

} //namespace evm_runtime