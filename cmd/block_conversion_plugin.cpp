#include "block_conversion_plugin.hpp"
#include "channels.hpp"
#include "abi_utils.hpp"
#include "utils.hpp"
#include "contract_common/evm_common/block_mapping.hpp"

#include <fstream>

#include <silkworm/types/transaction.hpp>
#include <silkworm/trie/vector_root.hpp>
#include <silkworm/common/endian.hpp>

using sys = sys_plugin;

silkworm::log::BufferBase& operator << ( silkworm::log::BufferBase& ss, const eosio::checksum256& id ) {
   ss << silkworm::to_hex(id.extract_as_byte_array());
   return ss;
}

silkworm::log::BufferBase& operator << ( silkworm::log::BufferBase& ss, const channels::native_block& block ) {
   ss << "#" << block.block_num << " ("
      << "id:" << block.id << ","
      << "prev:" << block.prev
      << ")";
   return ss;
}

silkworm::log::BufferBase& operator << ( silkworm::log::BufferBase& ss, const silkworm::Block& block ) {
   ss << "#" << block.header.number << ", txs:" << block.transactions.size() << ", hash:" << silkworm::to_hex(block.header.hash());
   return ss;
}

class block_conversion_plugin_impl : std::enable_shared_from_this<block_conversion_plugin_impl> {
   public:
      block_conversion_plugin_impl()
        : evm_blocks_channel(appbase::app().get_channel<channels::evm_blocks>()){}


      uint32_t timestamp_to_evm_block_num(uint64_t timestamp) const {
         if (timestamp < bm.value().genesis_timestamp) {
            SILK_CRIT << "Invalid timestamp " << timestamp << ", genesis: " << bm->genesis_timestamp;
            assert(timestamp >= bm->genesis_timestamp);
         }

         return bm->timestamp_to_evm_block_num(timestamp);
      }

      void load_head() {
         auto head_block = appbase::app().get_plugin<engine_plugin>().get_head_block();
         if (!head_block) {
            sys::error("Unable to read head block");
            return;
         }
         SILK_INFO << "load_head: " << *head_block;
         evm_blocks.push_back(*head_block);

         channels::native_block nb;
         std::optional<channels::native_block> start_from_block = appbase::app().get_plugin<ship_receiver_plugin>().get_start_from_block();
         if( start_from_block ) {
            nb = *start_from_block;
         } else {
            nb.id = eosio::checksum256(head_block->header.mix_hash.bytes);
            nb.block_num = utils::to_block_num(head_block->header.mix_hash.bytes);
            nb.timestamp = head_block->header.timestamp*1e6;
         }
         SILK_INFO << "Loaded native block: [" << head_block->header.number << "][" << nb.block_num << "],[" << nb.timestamp << "]";
         native_blocks.push_back(nb);

         auto genesis_header = appbase::app().get_plugin<engine_plugin>().get_genesis_header();
         if (!genesis_header) {
            sys::error("Unable to read genesis header");
            return;
         }

         bm.emplace(genesis_header->timestamp, 1); // Hardcoded to 1 second block interval

         SILK_INFO << "Block interval (in seconds): " << bm->block_interval;
         SILK_INFO << "Genesis timestamp (in seconds since Unix epoch): " << bm->genesis_timestamp;

         // The nonce in the genesis header encodes the name of the Antelope account on which the EVM contract has been deployed.
         // This name is necessary to determine which reserved address to use as the beneficiary of the blocks.
         evm_contract_name = silkworm::endian::load_big_u64(genesis_header->nonce.data());

         SILK_INFO << "Genesis nonce (as hex): " << silkworm::to_hex(evm_contract_name, true);
         SILK_INFO << "Genesis nonce (as Antelope name): " << eosio::name{evm_contract_name}.to_string();

         if (evm_contract_name == 0  || (evm_contract_name == 1000)) {
            // TODO: Remove the (evm_contract_name == 1000) condition once we feel comfortable other tests and scripts have been 
            //       updated to reflect this new meaning of the nonce (used to be block interval in milliseconds).

            SILK_CRIT << "Genesis nonce does not represent a valid Antelope account name. "
                         "It must be the name of the account on which the EVM contract is deployed";
            sys::error("Invalid genesis nonce");
            return;
         }
      }

      evmc::bytes32 compute_transaction_root(const silkworm::BlockBody& body) {
         if (body.transactions.empty()) {
            return silkworm::kEmptyRoot;
         }

         static constexpr auto kEncoder = [](silkworm::Bytes& to, const silkworm::Transaction& txn) {
            silkworm::rlp::encode(to, txn, /*for_signing=*/false, /*wrap_eip2718_into_string=*/false);
         };

         evmc::bytes32 txn_root{silkworm::trie::root_hash(body.transactions, kEncoder)};

         return txn_root;
      }

      void log_internal_status(const std::string& label) {
         SILK_INFO << "internal_status(" << label << "): nb:" << native_blocks.size() << ", evmb:" << evm_blocks.size();
      }

      silkworm::Block generate_new_evm_block(uint64_t num, const evmc::bytes32& parent_hash) {
         silkworm::Block new_block;
         evm_common::prepare_block_header(new_block.header, bm.value(), evm_contract_name, num);
         new_block.header.parent_hash       = parent_hash;
         new_block.header.transactions_root = silkworm::kEmptyRoot;
         //new_block.header.mix_hash
         //new_block.header.nonce           
         //new_block.header.ommers_hash;
         //new_block.header.beneficiary;
         //new_block.header.state_root;
         //new_block.header.receipts_root;
         //new_block.header.logs_bloom;
         //new_block.header.gas_used;
         //new_block.header.extra_data;
         return new_block;
      }

      // Set the mix_hash header field of the `evm_block` to the `native_block` id
      void set_upper_bound(silkworm::Block& evm_block, const channels::native_block& native_block) {
         auto id = native_block.id.extract_as_byte_array();
         static_assert(sizeof(decltype(id)) == sizeof(decltype(evm_block.header.mix_hash.bytes)));
         std::copy(id.begin(), id.end(), evm_block.header.mix_hash.bytes);
         evm_block.irreversible = native_block.block_num <= native_block.lib;
      }

      template <typename F>
      void for_each_action(const channels::native_block& block, F&& f) {
         for(const auto& trx: block.transactions) {
            for(const auto& act: trx.actions) {
               f(act);
            }
         }
      }

      template <typename F>
      void for_each_reverse_action(const channels::native_block& block, F&& f) {
         for(auto trx=block.transactions.rbegin(); trx != block.transactions.rend(); ++trx) {
            for(auto act=trx->actions.rbegin(); act != trx->actions.rend(); ++act) {
               f(*act);
            }
         }
      }

      inline void init() {
         SILK_DEBUG << "block_conversion_plugin_impl INIT";
         load_head();
         
         native_blocks_subscription = appbase::app().get_channel<channels::native_blocks>().subscribe(
            [this](auto new_block) {
               static size_t count = 0;

               // Keep the last block before genesis timestamp
               if (new_block->timestamp <= bm.value().genesis_timestamp) {
                  SILK_WARN << "Before genesis: " << bm->genesis_timestamp <<  " Block #" << new_block->block_num << " timestamp: " << new_block->timestamp;
                  native_blocks.clear();
                  native_blocks.push_back(*new_block);
                  return;
               }

               // Check if received native block can't be linked
               if( !native_blocks.empty() && native_blocks.back().id != new_block->prev ) {

                  SILK_WARN << "Can't link new block " << *new_block;

                  // Find fork block
                  auto fork_block = std::find_if(native_blocks.begin(), native_blocks.end(), [&new_block](const auto& nb){ return nb.id == new_block->prev; });
                  if( fork_block == native_blocks.end() ) {
                     SILK_CRIT << "Unable to find fork block " << new_block->prev;
                     throw std::runtime_error("Unable to find fork block");
                  }

                  SILK_WARN << "Fork at Block " << *fork_block;

                  // Remove EVM blocks after the fork
                  while( !evm_blocks.empty() && timestamp_to_evm_block_num(fork_block->timestamp) < evm_blocks.back().header.number ) {
                     SILK_WARN << "Removing forked EVM block " << evm_blocks.back();
                     evm_blocks.pop_back();
                  }

                  // Remove forked native blocks up until the fork point
                  while( !native_blocks.empty() && native_blocks.back().id != fork_block->id ) {

                     // Check if the native block to be removed has transactions
                     // and they belong to the EVM block of the fork point
                     if( native_blocks.back().transactions.size() > 0 && timestamp_to_evm_block_num(native_blocks.back().timestamp) == timestamp_to_evm_block_num(fork_block->timestamp) ) {

                        // Check that we can remove transactions contained in the forked native block
                        if (evm_blocks.empty() || timestamp_to_evm_block_num(native_blocks.back().timestamp) != evm_blocks.back().header.number) {
                           SILK_CRIT << "Unable to remove transactions"
                                       << "(empty: " << evm_blocks.empty()
                                       << ", evmblock(native):" << timestamp_to_evm_block_num(native_blocks.back().timestamp) <<")"
                                       << ", evm number:" << evm_blocks.back().header.number <<")";
                           throw std::runtime_error("Unable to remove transactions");
                        }

                        // Remove transactions in forked native block
                        SILK_WARN << "Removing transactions in forked native block  " << native_blocks.back();
                        for_each_reverse_action(native_blocks.back(), [this](const auto& act){
                              auto dtx = deserialize_tx(act.data);
                              auto txid_a = ethash::keccak256(dtx.rlpx.data(), dtx.rlpx.size());

                              silkworm::Bytes transaction_rlp{};
                              silkworm::rlp::encode(transaction_rlp, evm_blocks.back().transactions.back());
                              auto txid_b = ethash::keccak256(transaction_rlp.data(), transaction_rlp.size());

                              // Ensure that the transaction to be removed is the correct one
                              if( std::memcmp(txid_a.bytes, txid_b.bytes, sizeof(txid_a.bytes)) != 0) {
                                 SILK_CRIT << "Unable to remove transaction "
                                             << ",txid_a:" << silkworm::to_hex(txid_a.bytes)
                                             << ",txid_b:" << silkworm::to_hex(txid_b.bytes);
                                 throw std::runtime_error("Unable to remove transaction");
                              }

                              SILK_WARN << "Removing trx: " << silkworm::to_hex(txid_a.bytes);
                              evm_blocks.back().transactions.pop_back();
                        });
                     }

                     // Remove forked native block
                     SILK_WARN << "Removing forked native block " << native_blocks.back();
                     native_blocks.pop_back();
                  }

                  // Ensure upper bound native block correspond to this EVM block
                  if( evm_blocks.empty() || timestamp_to_evm_block_num(native_blocks.back().timestamp) != evm_blocks.back().header.number ) {
                     SILK_CRIT << "Unable to set upper bound "
                                 << "(empty: " << evm_blocks.empty()
                                 << ", evmblock(native):" << timestamp_to_evm_block_num(native_blocks.back().timestamp) <<")"
                                 << ", evm number:" << evm_blocks.back().header.number <<")";
                     throw std::runtime_error("Unable to set upper bound");
                  }

                  // Reset upper bound
                  SILK_WARN << "Reset upper bound for EVM Block " << evm_blocks.back() << " to: " << native_blocks.back();
                  set_upper_bound(evm_blocks.back(), native_blocks.back());
               }

               // Enqueue received block
               native_blocks.push_back(*new_block);

               // Extend the EVM chain if necessary up until the block where the received block belongs
               auto evm_num = timestamp_to_evm_block_num(new_block->timestamp);

               while(evm_blocks.back().header.number < evm_num) {
                  auto& last_evm_block = evm_blocks.back();
                  last_evm_block.header.transactions_root = compute_transaction_root(last_evm_block);
                  evm_blocks_channel.publish(80, std::make_shared<silkworm::Block>(last_evm_block));
                  evm_blocks.push_back(generate_new_evm_block(last_evm_block.header.number+1, last_evm_block.header.hash()));
               }

               // Add transactions to the evm block
               auto& curr = evm_blocks.back();
               for_each_action(*new_block, [this, &curr](const auto& act){
                     auto dtx = deserialize_tx(act.data);
                     silkworm::ByteView bv = {(const uint8_t*)dtx.rlpx.data(), dtx.rlpx.size()};
                     silkworm::Transaction evm_tx;
                     if (silkworm::rlp::decode<silkworm::Transaction>(bv, evm_tx) != silkworm::DecodingResult::kOk) {
                        SILK_CRIT << "Failed to decode transaction in block: " << curr.header.number;
                        throw std::runtime_error("Failed to decode transaction");
                     }
                     curr.transactions.emplace_back(std::move(evm_tx));
               });
               set_upper_bound(curr, *new_block);

               // Calculate last irreversible EVM block
               std::optional<uint64_t> lib_timestamp;
               auto it = std::upper_bound(native_blocks.begin(), native_blocks.end(), new_block->lib, [](uint32_t lib, const auto& nb) { return lib < nb.block_num; });
               if(it != native_blocks.begin()) {
                  --it;
                  lib_timestamp = it->timestamp;
               }

               if( lib_timestamp ) {
                  auto evm_lib = timestamp_to_evm_block_num(*lib_timestamp) - 1;

                  // Remove irreversible native blocks
                  while(timestamp_to_evm_block_num(native_blocks.front().timestamp) < evm_lib) {
                     native_blocks.pop_front();
                  }

                  // Remove irreversible evm blocks
                  while(evm_blocks.front().header.number < evm_lib) {
                     evm_blocks.pop_front();
                  }
               }
            }
         );
      }

      inline pushtx deserialize_tx(const std::vector<char>& d) const {
         pushtx tx;
         eosio::convert_from_bin(tx, d);
         return tx;
      }

      void shutdown() {}

      std::list<channels::native_block>             native_blocks;
      std::list<silkworm::Block>                    evm_blocks;
      channels::evm_blocks::channel_type&           evm_blocks_channel;
      channels::native_blocks::channel_type::handle native_blocks_subscription;
      std::optional<evm_common::block_mapping>      bm;
      uint64_t                                      evm_contract_name = 0;
};

block_conversion_plugin::block_conversion_plugin() : my(new block_conversion_plugin_impl()) {}
block_conversion_plugin::~block_conversion_plugin() {}

void block_conversion_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {

}

void block_conversion_plugin::plugin_initialize( const appbase::variables_map& options ) {
   my->init();
   SILK_INFO << "Initialized block_conversion Plugin";
}

void block_conversion_plugin::plugin_startup() {
}

void block_conversion_plugin::plugin_shutdown() {
   my->shutdown();
   SILK_INFO << "Shutdown block_conversion plugin";
}
