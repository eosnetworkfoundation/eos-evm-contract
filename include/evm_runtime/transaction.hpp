#pragma once

#include <optional>
#include <eosio/eosio.hpp>
#include <evm_runtime/types.hpp>
#include <silkworm/core/rlp/encode.hpp>
#include <silkworm/core/types/transaction.hpp>

namespace evm_runtime {

using silkworm::Bytes;
using silkworm::ByteView;

struct transaction {

  transaction() = delete;
  explicit transaction(bytes rlptx) : rlptx_(std::move(rlptx)) {}
  explicit transaction(silkworm::Transaction tx) : tx_(std::move(tx)) {}

  const bytes& get_rlptx()const {
    if(!rlptx_) {
      eosio::check(tx_.has_value(), "no tx");
      Bytes rlp;
      silkworm::rlp::encode(rlp, tx_.value());
      rlptx_.emplace(bytes{rlp.begin(), rlp.end()});
    }
    return rlptx_.value();
  }

  const silkworm::Transaction& get_tx()const {
    if(!tx_) {
      eosio::check(rlptx_.has_value(), "no rlptx");
      ByteView bv{(const uint8_t*)rlptx_->data(), rlptx_->size()};
      silkworm::Transaction tmp;
      eosio::check(silkworm::rlp::decode_transaction(bv, tmp, silkworm::rlp::Eip2718Wrapping::kNone) && bv.empty(), "unable to decode transaction");
      tx_.emplace(tmp);
    }
    return tx_.value();
  }

  void recover_sender()const {
    eosio::check(tx_.has_value(), "no tx");
    auto& tx = tx_.value();
    tx.from.reset();
    tx.recover_sender();
  }

private:
  mutable std::optional<bytes>  rlptx_;
  mutable std::optional<silkworm::Transaction> tx_;
};

} //namespace evm_runtime
