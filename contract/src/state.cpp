#include <map>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/state.hpp>
#include <ethash/keccak.hpp>
#include <silkworm/common/util.hpp>
#include <evm_runtime/intrinsics.hpp>

namespace evm_runtime {

std::optional<Account> state::read_account(const evmc::address& address) const noexcept {    
    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(address));
    ++stats.account.read;

    if (itr == inx.end()) {
        return {};
    }
    
    auto code_hash = itr->get_code_hash();
    addr2id[address] = itr->id;
    addr2code[code_hash] = itr->code;

    return Account{itr->nonce, intx::be::load<uint256>(itr->get_balance()), code_hash, 0};
}

ByteView state::read_code(const evmc::bytes32& code_hash) const noexcept {
    
    if(addr2code.find(code_hash) != addr2code.end()) {
        const auto& code = addr2code[code_hash];
        return ByteView{(const uint8_t*)code.data(), code.size()};
    }
    
    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.codehash"_n>();
    auto itr = inx.find(make_key(code_hash));
    ++stats.account.read;

    if (itr == inx.end() || itr->code.size() == 0) {
        return ByteView{};
    }

    addr2code[code_hash] = itr->code;
    const auto& code = addr2code[code_hash];
    return ByteView{(const uint8_t*)code.data(), code.size()};
}

evmc::bytes32 state::read_storage(const evmc::address& address, uint64_t incarnation,
                                          const evmc::bytes32& location) const noexcept {
    
    uint64_t account_id = 0;
    if(addr2id.find(address) == addr2id.end()) {
        account_table accounts(_self, _self.value);
        auto inx = accounts.get_index<"by.address"_n>();
        auto itr = inx.find(make_key(address));
        ++stats.account.read;
        if (itr == inx.end()) return {};
        addr2id[address] = itr->id;
    }

    account_id = addr2id[address];

    storage_table db(_self, account_id);
    auto inx2 = db.get_index<"by.key"_n>();
    auto itr2 = inx2.find(make_key(location));
    ++stats.storage.read;
    
    if(itr2 == inx2.end()) return {};

    evmc::bytes32 res;
    std::copy(itr2->value.begin(), itr2->value.end(), res.bytes);

    return res;
}

uint64_t state::previous_incarnation(const evmc::address& address) const noexcept {
    return 0;
}

void state::begin_block(uint64_t block_number) {}

void state::update_account(const evmc::address& address, std::optional<Account> initial,
                                   std::optional<Account> current) {

    const bool equal{current == initial};
    if(equal) return;

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(address));
    ++stats.account.read;

    if (current.has_value()) {
        if (itr == inx.end()) {
            accounts.emplace(_ram_payer, [&](auto& row){
                row.id = accounts.available_primary_key();
                row.eth_address = to_bytes(address);
                row.nonce = current->nonce;
                row.balance = to_bytes(current->balance);
                row.code_hash = to_bytes(current->code_hash);
            });
            ++stats.account.create;
        } else {
            accounts.modify(*itr, eosio::same_payer, [&](auto& row){
                row.nonce = current->nonce;
                row.code_hash = to_bytes(current->code_hash);
                row.balance = to_bytes(current->balance);
            });
            ++stats.account.update;
        }
    } else {
        if(itr != inx.end()) {
            storage_table db(_self, itr->id);
            auto sitr = db.begin();
            while( sitr != db.end() ) {
                sitr = db.erase(sitr);
                ++stats.storage.remove;
            }
            accounts.erase(*itr);
            ++stats.account.remove;
        }
    }
}

void state::update_account_code(const evmc::address& address, uint64_t, const evmc::bytes32& code_hash, ByteView code) {
    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(address));
    ++stats.account.read;
    if( itr != inx.end() ) {
        accounts.modify(*itr, eosio::same_payer, [&](auto& row){
            row.code = bytes{code.begin(), code.end()};
            row.code_hash = to_bytes(code_hash);
        });
        ++stats.account.update;
    } else {
        accounts.emplace(_ram_payer, [&](auto& row){
            row.id = accounts.available_primary_key();;
            row.eth_address = to_bytes(address);
            row.nonce = 0;
            row.code = bytes{code.begin(), code.end()};
            row.code_hash = to_bytes(code_hash);
        });
        ++stats.account.create;
    }
}

void state::update_storage(const evmc::address& address, uint64_t incarnation, const evmc::bytes32& location,
                                   const evmc::bytes32& initial, const evmc::bytes32& current) {
    
    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(address));
    ++stats.account.read;

    if (is_zero(current)) {
        if(itr == inx.end()) return;
        storage_table db(_self, itr->id);
        auto inx2 = db.get_index<"by.key"_n>();
        auto itr2 = inx2.find(make_key(location));
        ++stats.storage.read;
        if(itr2 == inx2.end()) return;
        db.erase(*itr2);
        ++stats.storage.remove;
    } else {
        uint64_t table_id;
        if(itr == inx.end()){
            accounts.emplace(_ram_payer, [&](auto& row){
                table_id = accounts.available_primary_key();
                row.id = table_id;
                row.eth_address = to_bytes(address);
                row.nonce = 0;
                row.code_hash = to_bytes(kEmptyHash);
            });
            ++stats.account.read;
        } else {
            table_id = itr->id;
        }

        storage_table db(_self, table_id);
        auto inx2 = db.get_index<"by.key"_n>();
        auto itr2 = inx2.find(make_key(location));
        ++stats.storage.read;
        if(itr2 == inx2.end()) {
            db.emplace(_ram_payer, [&](auto& row){
                row.id = db.available_primary_key();
                row.key = to_bytes(location);
                row.value = to_bytes(current);
            });
            ++stats.storage.create;
        } else {
            db.modify(*itr2, eosio::same_payer, [&](auto& row){
                row.value = to_bytes(current);
            });
            ++stats.storage.update;
        }
    }
}

std::optional<BlockHeader> state::read_header(uint64_t block_number,
                                                      const evmc::bytes32& block_hash) const noexcept {
    eosio::check(false, "read_header not implemented");
    return {};
}

bool state::read_body(BlockNum block_number, const evmc::bytes32& block_hash,
                                        BlockBody& out) const noexcept {
    eosio::check(false, "read_body not implemented");
    return {};
}

std::optional<intx::uint256> state::total_difficulty(uint64_t block_number,
                                                             const evmc::bytes32& block_hash) const noexcept {
    eosio::check(false, "total_difficulty not implemented");
    return {};
}

uint64_t state::current_canonical_block() const {
    eosio::check(false, "current_canonical_block not implemented");
    return {};
}

std::optional<evmc::bytes32> state::canonical_hash(uint64_t block_number) const {
    eosio::check(false, "canonical_hash not implemented");
    return {};
}

void state::insert_block(const Block& block, const evmc::bytes32& hash) {
    eosio::check(false, "insert_block not implemented");
}

void state::canonize_block(uint64_t block_number, const evmc::bytes32& block_hash) {
    eosio::check(false, "canonize_block not implemented");
}

void state::decanonize_block(uint64_t block_number) { 
    eosio::check(false, "decanonize_block not implemented"); 
}

void state::insert_receipts(uint64_t, const std::vector<Receipt>&) {
    eosio::check(false, "insert_receipts not implemented");
}

void state::unwind_state_changes(uint64_t block_number) {
    eosio::check(false, "unwind_state_changes not implemented");
}

evmc::bytes32 state::state_root_hash() const {
    eosio::check(false, "state_root_hash not implemented");
    return {};
}

}  // namespace evm_runtime
