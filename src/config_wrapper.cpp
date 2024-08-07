#include <evm_runtime/tables.hpp>
#include <evm_runtime/config_wrapper.hpp>

namespace evm_runtime {

config_wrapper::config_wrapper(eosio::name self) : _self(self), _config(self, self.value) {
    _exists = _config.exists();
    if(_exists) {
        _cached_config = _config.get();
    }
    if (!_cached_config.evm_version.has_value()) {
        _cached_config.evm_version = value_promoter_evm_version_type{};
        // Don't set dirty because action can be read-only.
    }
    if (!_cached_config.consensus_parameter.has_value()) {
        _cached_config.consensus_parameter = value_promoter_consensus_parameter_data_type{};
        // Don't set dirty because action can be read-only.
    }
    if (!_cached_config.token_contract.has_value()) {
        _cached_config.token_contract = default_token_account;
        // Don't set dirty because action can be read-only.
    }
    if (!_cached_config.queue_front_block.has_value()) {
        _cached_config.queue_front_block = 0;
    }
    if (!_cached_config.gas_prices.has_value()) {
        _cached_config.gas_prices = gas_prices_type{};
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
    eosio::check(evm_precision >= ingress_bridge_fee.symbol.precision(), "invalid ingress fee precision");
    _cached_config.ingress_bridge_fee = ingress_bridge_fee;
    set_dirty();
}

uint64_t config_wrapper::get_gas_price()const {
    return _cached_config.gas_price;
}

void config_wrapper::set_gas_price(uint64_t gas_price) {
    _cached_config.gas_price = gas_price;
    set_dirty();
}

gas_prices_type config_wrapper::get_gas_prices()const {
    return *_cached_config.gas_prices;
}

void config_wrapper::set_gas_prices(const gas_prices_type& prices) {
    _cached_config.gas_prices = prices;
    set_dirty();
}

template <typename Q, typename Func>
void config_wrapper::enqueue(Func&& update_fnc) {
    Q queue(_self, _self.value);
    auto activation_time = get_current_time() + eosio::seconds(grace_period_seconds);

    eosevm::block_mapping bm(get_genesis_time().sec_since_epoch());
    auto activation_block_num = bm.timestamp_to_evm_block_num(activation_time.time_since_epoch().count()) + 1;

    auto it = queue.end();
    if( it != queue.begin()) {
        --it;
        eosio::check(activation_block_num >= it->block, "internal error");
        if(activation_block_num == it->block) {
            queue.modify(*it, eosio::same_payer, [&](auto& el) {
                update_fnc(el);
            });
            return;
        }
    }

    queue.emplace(_self, [&](auto& el) {
        el.block = activation_block_num;
        update_fnc(el);
    });

    if( _cached_config.queue_front_block.value() == 0 ) {
        set_queue_front_block(activation_block_num);
    }
}

void config_wrapper::enqueue_gas_price(uint64_t gas_price) {
    enqueue<price_queue_table>([&](auto& el){
        el.price = gas_price;
    });
}

void config_wrapper::enqueue_gas_prices(const gas_prices_type& prices) {
    enqueue<prices_queue_table>([&](auto& el){
        el.prices = prices;
    });
}

void config_wrapper::set_queue_front_block(uint32_t block_num) {
    _cached_config.queue_front_block = block_num;
    set_dirty();
}


void config_wrapper::process_price_queue() {
    if( get_evm_version() >= 3) {
        process_queue<prices_queue_table>([&](const auto& row){
            set_gas_prices(row.prices);
        });
    } else {
        process_queue<price_queue_table>([&](const auto& row){
            set_gas_price(row.price);
        });
    }
}

template <typename Q, typename Func>
void config_wrapper::process_queue(Func&& update_func) {
    eosevm::block_mapping bm(get_genesis_time().sec_since_epoch());
    auto current_block_num = bm.timestamp_to_evm_block_num(get_current_time().time_since_epoch().count());

    auto queue_front_block = _cached_config.queue_front_block.value();
    if( queue_front_block == 0 || current_block_num < queue_front_block ) {
        return;
    }

    Q queue(_self, _self.value);
    auto it = queue.begin();
    while( it != queue.end() && current_block_num >= it->block ) {
        update_func(*it);
        it = queue.erase(it);
        set_queue_front_block(it != queue.end() ? it->block : 0);
    }
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
    // should not happen
    eosio::check(_cached_config.evm_version.has_value(), "evm_version not exist");
    return _cached_config.evm_version->get_value(_cached_config.genesis_time, get_current_time());
}

uint64_t config_wrapper::get_evm_version_and_maybe_promote() {
    uint64_t current_version = 0;
    bool promoted = false;
    if(_cached_config.evm_version.has_value()) {
        std::tie(current_version, promoted) = _cached_config.evm_version->get_value_and_maybe_promote(_cached_config.genesis_time, get_current_time());
    }
    if(promoted) {
        if(current_version >=1 && _cached_config.miner_cut != 0) _cached_config.miner_cut = 0;
        set_dirty();
    }
    return current_version;
}

void config_wrapper::set_evm_version(uint64_t new_version) {
    eosio::check(new_version <= eosevm::max_eos_evm_version, "Unsupported version");
    eosio::check(new_version != 3 || _cached_config.queue_front_block.value() == 0, "price queue must be empty");
    auto current_version = get_evm_version_and_maybe_promote();
    eosio::check(new_version > current_version, "new version must be greater than the active one");
    _cached_config.evm_version->update([&](auto& v) {
        v = new_version;
    }, _cached_config.genesis_time, get_current_time());
    set_dirty();
}

void config_wrapper::set_fee_parameters(const fee_parameters& fee_params,
                        bool allow_any_to_be_unspecified)
{
    if (fee_params.gas_price.has_value()) {
        eosio::check(*fee_params.gas_price >= one_gwei, "gas_price must >= 1Gwei");
        auto current_version = get_evm_version_and_maybe_promote();
        if( current_version >= 1 ) {
            eosio::check(current_version < 3, "can't set gas_price");
            enqueue_gas_price(*fee_params.gas_price);
        } else {
            set_gas_price(*fee_params.gas_price);
        }
    } else {
        eosio::check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing gas_price");
    }

    if (fee_params.miner_cut.has_value()) {
        eosio::check(get_evm_version() == 0, "can't set miner_cut");
        eosio::check(*fee_params.miner_cut <= ninety_percent, "miner_cut must <= 90%");
        _cached_config.miner_cut = *fee_params.miner_cut;
    } else {
        eosio::check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing miner_cut");
    }

    if (fee_params.ingress_bridge_fee.has_value()) {
        if (_cached_config.ingress_bridge_fee.symbol != eosio::symbol()) {
            eosio::check(fee_params.ingress_bridge_fee->symbol == _cached_config.ingress_bridge_fee.symbol, "bridge symbol can't change");
        }
        eosio::check(fee_params.ingress_bridge_fee->amount >= 0, "ingress bridge fee cannot be negative");

        set_ingress_bridge_fee(*fee_params.ingress_bridge_fee);
    } else {
        eosio::check(allow_any_to_be_unspecified, "All required fee parameters not specified: missing ingress_bridge_fee");
    }

    set_dirty();
}

void config_wrapper::update_consensus_parameters(eosio::asset ram_price_mb, uint64_t gas_price) {
    eosio::check(get_evm_version() < 3, "unable to set params");

    //TODO: should we allow to call this when version>=3
    eosio::check(ram_price_mb.symbol == get_token_symbol(), "invalid price symbol");
    eosio::check(gas_price >= one_gwei, "gas_price must >= 1Gwei");

    auto miner_cut = get_evm_version() >= 1 ? 0 : _cached_config.miner_cut;
    double gas_per_byte_f = (ram_price_mb.amount / (1024.0 * 1024.0) * get_minimum_natively_representable()) / (gas_price * static_cast<double>(hundred_percent - miner_cut) / hundred_percent);

    constexpr uint64_t account_bytes = 347;
    constexpr uint64_t contract_fixed_bytes = 606;
    constexpr uint64_t storage_slot_bytes = 346;

    constexpr uint64_t max_gas_per_byte = (1ull << 43) - 1;

    eosio::check(gas_per_byte_f >= 0.0, "gas_per_byte must >= 0");
    eosio::check(gas_per_byte_f * contract_fixed_bytes < (double)(max_gas_per_byte), "gas_per_byte too big");

    uint64_t gas_per_byte = (uint64_t)(gas_per_byte_f + 1.0);

    this->update_consensus_parameters2(account_bytes * gas_per_byte, /* gas_txnewaccount */
                             account_bytes * gas_per_byte, /* gas_newaccount */
                             contract_fixed_bytes * gas_per_byte, /*gas_txcreate*/
                             gas_per_byte,/*gas_codedeposit*/
                             gas_sset_min + storage_slot_bytes * gas_per_byte /*gas_sset*/
    );

    if(get_evm_version() >= 1) {
        enqueue_gas_price(gas_price);
    } else {
        set_gas_price(gas_price);
    }
}

void config_wrapper::update_consensus_parameters2(std::optional<uint64_t> gas_txnewaccount, std::optional<uint64_t> gas_newaccount, std::optional<uint64_t> gas_txcreate, std::optional<uint64_t> gas_codedeposit, std::optional<uint64_t> gas_sset)
{
    eosio::check(get_evm_version() >= 1, "evm_version must >= 1");

    // should not happen
    eosio::check(_cached_config.consensus_parameter.has_value(), "consensus_parameter not exist");

    _cached_config.consensus_parameter->update([&](auto& p) {
        std::visit([&](auto& v){
            if (gas_txnewaccount.has_value()) v.gas_parameter.gas_txnewaccount = *gas_txnewaccount;
            if (gas_newaccount.has_value()) v.gas_parameter.gas_newaccount = *gas_newaccount;
            if (gas_txcreate.has_value()) v.gas_parameter.gas_txcreate = *gas_txcreate;
            if (gas_codedeposit.has_value()) v.gas_parameter.gas_codedeposit = *gas_codedeposit;
            if (gas_sset.has_value()) {
                eosio::check(*gas_sset >= gas_sset_min, "gas_sset too small");
                v.gas_parameter.gas_sset = *gas_sset;
            }
        }, p);
    }, _cached_config.genesis_time, get_current_time());

    set_dirty();
}

const consensus_parameter_data_type& config_wrapper::get_consensus_param() {
    // should not happen
    eosio::check(_cached_config.consensus_parameter.has_value(), "consensus_parameter not exist");
    return _cached_config.consensus_parameter->get_value(_cached_config.genesis_time, get_current_time());
}

std::pair<const consensus_parameter_data_type &, bool> config_wrapper::get_consensus_param_and_maybe_promote() {

    // should not happen
    eosio::check(_cached_config.consensus_parameter.has_value(), "consensus_parameter not exist");

    auto pair = _cached_config.consensus_parameter->get_value_and_maybe_promote(_cached_config.genesis_time, get_current_time());
    if (pair.second) {
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

void config_wrapper::set_token_contract(eosio::name token_contract) {
    _cached_config.token_contract = token_contract;
}

eosio::name config_wrapper::get_token_contract() const {
    return *_cached_config.token_contract;
}

eosio::symbol config_wrapper::get_token_symbol() const {
    return _cached_config.ingress_bridge_fee.symbol;
}

uint64_t config_wrapper::get_minimum_natively_representable() const {
    return pow10_const(evm_precision - _cached_config.ingress_bridge_fee.symbol.precision());
}

} //namespace evm_runtime
