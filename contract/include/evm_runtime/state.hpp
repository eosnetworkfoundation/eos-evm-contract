#pragma once

#include <vector>
#include <map>
#include <eosio/eosio.hpp>
#include <evm_runtime/types.hpp>
#include <silkworm/state/state.hpp>

namespace evm_runtime {

using namespace silkworm;
using namespace eosio;

struct table_stats {
    uint32_t read=0;
    uint32_t update=0;
    uint32_t create=0;
    uint32_t remove=0;
};

struct db_stats {
    table_stats account;
    table_stats storage;
};

struct state : State {
    name _self;
    name _ram_payer;
    mutable std::map<evmc::address, uint64_t> addr2id;
    mutable std::map<bytes32, bytes> addr2code;
    mutable db_stats stats;

    explicit state(name self, name ram_payer) : _self(self), _ram_payer(ram_payer){}

    std::optional<Account> read_account(const evmc::address& address) const noexcept override;

    ByteView read_code(const evmc::bytes32& code_hash) const noexcept override;

    evmc::bytes32 read_storage(const evmc::address& address, uint64_t incarnation,
                               const evmc::bytes32& location) const noexcept override;

    uint64_t previous_incarnation(const evmc::address& address) const noexcept override;

    std::optional<BlockHeader> read_header(uint64_t block_number,
                                           const evmc::bytes32& block_hash) const noexcept override;

    bool read_body(BlockNum block_number, const evmc::bytes32& block_hash,
                                            BlockBody& out) const noexcept override;

    std::optional<intx::uint256> total_difficulty(uint64_t block_number,
                                                  const evmc::bytes32& block_hash) const noexcept override;

    evmc::bytes32 state_root_hash() const override;

    uint64_t current_canonical_block() const override;

    std::optional<evmc::bytes32> canonical_hash(uint64_t block_number) const override;

    void insert_block(const Block& block, const evmc::bytes32& hash) override;

    void canonize_block(uint64_t block_number, const evmc::bytes32& block_hash) override;

    void decanonize_block(uint64_t block_number) override;

    void insert_receipts(uint64_t block_number, const std::vector<Receipt>& receipts) override;

    void begin_block(uint64_t block_number) override;

    void update_account(const evmc::address& address, std::optional<Account> initial,
                        std::optional<Account> current) override;

    /// @return true if all garbage has been collected
    bool gc(uint32_t max);

    void update_account_code(const evmc::address& address, uint64_t incarnation, const evmc::bytes32& code_hash,
                             ByteView code) override;

    void update_storage(const evmc::address& address, uint64_t incarnation, const evmc::bytes32& location,
                        const evmc::bytes32& initial, const evmc::bytes32& current) override;

    void unwind_state_changes(uint64_t block_number) override;
};

}  // namespace evm_runtime

