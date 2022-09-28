#include "ship_receiver_plugin.hpp"
#include "abi_utils.hpp"
#include "chain_state.hpp"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/program_options.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/algorithm/string.hpp> 

namespace asio      = boost::asio;
namespace bio       = boost::iostreams;
namespace bpo       = boost::program_options;
namespace websocket = boost::beast::websocket;
namespace bip       = boost::interprocess;
namespace bfs       = boost::filesystem;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

using sys = sys_plugin;
class ship_receiver_plugin_impl : std::enable_shared_from_this<ship_receiver_plugin_impl> {
   public:
      ship_receiver_plugin_impl()
         : blocks_channel(appbase::app().get_channel<channels::native_blocks>()) {
         }

      using block_result_t = eosio::ship_protocol::get_blocks_result_v0;
      using native_block_t = channels::native_block;

      void init(std::string h, std::string p, uint32_t g, eosio::name ca, std::string_view cs_dir) {
         host = h;
         port = p;
         genesis = g;
         core_account = ca;
         core_chain_state = chain_state{{cs_dir.data(), cs_dir.size()}};
         resolver = std::make_shared<tcp::resolver>(appbase::app().get_io_service());
         stream   = std::make_shared<websocket::stream<tcp::socket>>(appbase::app().get_io_service());
         stream->binary(true);
         stream->read_message_max(0x1ull << 36);
         connect_stream();
         if (core_chain_state.exists()) {
            core_chain_state.read();
            if (core_chain_state.head_block_num > genesis)
               genesis = core_chain_state.head_block_num;
         }
      }

      void update_core_chain_state(uint32_t block_num) {
         core_chain_state.head_block_num = block_num;
         core_chain_state.write();
      }

      void update_trust_chain_state(uint32_t block_num) {
         core_chain_state.head_trust_block_num = block_num;
         core_chain_state.write();
      }

      inline chain_state get_chain_state() const { return core_chain_state; }

      void initial_read() {
         flat_buffer buff;
         boost::system::error_code ec;
         stream->read(buff, ec);
         if (ec) {
            SILK_CRIT << "SHiP initial read failed : " << ec.message();
            sys::error();
         }

         abi = load_abi(eosio::json_token_stream{(char*)buff.data().data()});
      }

      void send_request(const abieos::jvalue& v) {
         std::vector<char> bin;
         try {
            abieos::json_to_bin(bin, &get_type("request", abi), v, [&](){});
         } catch (std::runtime_error err) {
            sys::error(err.what());
         }
         boost::system::error_code ec;
         stream->write(asio::buffer(bin), ec);
         if (ec) {
            SILK_CRIT << "Sending request failed : " << ec.message();
            sys::error();
         }
      }

      void connect_stream() {
         boost::system::error_code ec;
         auto res = resolver->resolve( tcp::v4(), host, port, ec);
         if (ec) {
            SILK_CRIT << "Resolver failed : " << ec.message();
            sys::error();
         }

         boost::asio::connect(stream->next_layer(), res, ec);

         if (ec) {
            SILK_CRIT << "SHiP connection failed : " << ec.message();
            sys::error();
         }

         stream->handshake(host, "/", ec);

         if (ec) {
            SILK_CRIT << "SHiP failed handshake : " << ec.message();
            sys::error();
         }

         SILK_INFO << "Connected to SHiP at " << host << ":" << port;
      }

      inline auto read() const {
         auto buff = std::make_shared<flat_buffer>();
         boost::system::error_code ec;
         stream->read(*buff, ec);
         if (ec) {
            SILK_CRIT << "SHiP read failed : " << ec.message();
            sys::error();
         }
         return buff;
      }

      template <typename F>
      inline void async_read(F&& func) const {
         auto buff = std::make_shared<flat_buffer>();
         boost::system::error_code ec;

         stream->async_read(*buff, appbase::app().get_priority_queue().wrap(80,
            [this, buff, func](const auto ec, auto) {
               if (ec) {
                  SILK_CRIT << "SHiP read failed : " << ec.message();
                  sys::error();
               }
               func(buff);
            })
         );
      }


      void get_blocks_request(std::size_t start, std::size_t end) {
         abieos::jarray positions = {};
         bool fetch_block  = true;
         bool fetch_traces = true;
         bool fetch_deltas = false;

         send_request(
            abieos::jvalue{abieos::jarray{{"get_blocks_request_v0"},
               {abieos::jobject{
                  {{"start_block_num"}, {std::to_string(start)}},
                  {{"end_block_num"}, {std::to_string(end)}},
                  {{"max_messages_in_flight"}, {std::to_string(0xffffffff)}},
                  {{"have_positions"}, {positions}},
                  {{"irreversible_only"}, {false}},
                  {{"fetch_block"}, {fetch_block}},
                  {{"fetch_traces"}, {fetch_traces}},
                  {{"fetch_deltas"}, {fetch_deltas}},
               }}}});
      }

      void get_blocks_request() {
         get_blocks_request(received_blocks, received_blocks+1);
      } 

      template <typename Buffer>
      eosio::ship_protocol::get_blocks_result_v0 get_result(Buffer&& b){
         auto data = b->data();
         eosio::input_stream bin = {(const char*)data.data(), (const char*)data.data() + data.size()};
         verify_variant(bin, get_type("result", abi), "get_blocks_result_v0");

         auto block = eosio::from_bin<eosio::ship_protocol::get_blocks_result_v0>(bin);
         return block;
      }

      template <typename Buffer>
      std::optional<native_block_t> on_block_result(Buffer&& b) {
         native_block_t current_block;
         auto block = get_result(std::forward<Buffer>(b));

         if (!block.this_block) {
            return std::nullopt;
         }

         latest_height = block.this_block->block_num;

         received_blocks++;

         SILK_DEBUG << "on_block_result #" << block.this_block->block_num << ", received_blocks " << received_blocks;
         
         if (block.traces) {
            uint32_t num;
            eosio::varuint32_from_bin(num, *block.traces);
            //SILK_DEBUG << "Block #" << ptr->block_num << " with " << num << " transactions and produced by : " << sb.producer.to_string();
            for (std::size_t i = 0; i < num; i++) {
               eosio::ship_protocol::transaction_trace tt;
               try {
                  tt = eosio::from_bin<eosio::ship_protocol::transaction_trace>(*block.traces);
               } catch (const std::exception &e) {
                  SILK_ERROR << "failed to decode transaction_trace trace data remaining = " << block.traces->remaining() << ", " << e.what();
                  throw;
               }
               const auto& trace = std::get<eosio::ship_protocol::transaction_trace_v0>(tt);
               const auto& actions = trace.action_traces;
               for (std::size_t j=0; j < actions.size(); j++) {
                  const auto& act = std::get<eosio::ship_protocol::action_trace_v1>(actions[j]);
                  SILK_DEBUG << "act " << std::string(act.act.name) << " receiver " << std::string(act.receiver);
                  if (act.act.name == eosio::name("pushtx") && core_account == act.receiver) {
                     if (!current_block.building) {
                        
                        current_block = start_native_block(block);
                        current_block.building = true;
                     }
                     append_to_block(current_block, trace);
                  } 
               }
            }
         }

         current_block.lib = block.last_irreversible.block_num;

         return current_block.building ? std::optional<native_block_t>(std::move(current_block)) : std::nullopt;
      }

      template <typename BlockResult>
      inline native_block_t start_native_block(BlockResult&& res) const {
         SILK_INFO << "start native block " << res.this_block->block_num; 
         native_block_t block;
         block.block_num = res.this_block->block_num;
         eosio::ship_protocol::signed_block sb;
         eosio::from_bin(sb, *res.block);
         block.timestamp = sb.timestamp.to_time_point().time_since_epoch().count();
         SILK_DEBUG << "Leave native block " << block.block_num;
         return block;
      }

      template <typename Meta>
      inline void handle_tx_meta(Meta&& obj) {
         constexpr trust_evm::tx_meta_v0 system_version;
         if (std::holds_alternative<trust_evm::tx_meta_v0>(obj)) {
            const auto& txm = std::get<trust_evm::tx_meta_v0>(obj);
            if (!(txm.major == system_version.major && txm.minor == system_version.minor && txm.patch == system_version.patch)) {
               SILK_CRIT << "tx_meta version doesn't match system version system v"
                         << system_version.major << "." << system_version.minor << "." << system_version.patch
                         << " : tx_meta v" << txm.major << "." << txm.minor << "." << txm.patch;
               sys::error();
            }
         } else {
            SILK_CRIT << "tx_meta is an incompatible variant version";
            sys::error();
         }

      }

      template <typename Trx>
      inline native_block_t& append_to_block(native_block_t& block, Trx&& trx) {
         channels::native_trx native_trx = {trx.id, trx.cpu_usage_us, trx.elapsed};
         const auto& actions = trx.action_traces;
         SILK_DEBUG << "Appending transaction ";
         for (std::size_t j=0; j < actions.size(); j++) {
            const auto& act = std::get<eosio::ship_protocol::action_trace_v1>(actions[j]);
            eosio::input_stream retval = act.return_value;
            handle_tx_meta(eosio::from_bin<trust_evm::tx_meta>(retval));
            channels::native_action action = {
               act.action_ordinal,
               act.receiver,
               act.act.account,
               act.act.name,
               std::move(act.act.data)
            };
            native_trx.actions.emplace_back(std::move(action));
            SILK_DEBUG << "Appending action " << native_trx.actions.back().name.to_string();
         }
         block.transactions.emplace_back(std::move(native_trx));
         return block;
      }

     
      auto get_block_result(std::size_t s, std::size_t e){
         get_blocks_request(s, e);
         return get_result(read());
      }

      template <typename F>
      void get_block(std::size_t s, std::size_t e, F&& func){
         get_blocks_request(s, e);
         async_read([this, func](auto buff) {
            auto res = on_block_result(buff);
            func(res);
         });
      }

      void shutdown() {
      }

      void read_next_block() {
         async_read([this](auto buff) {
            std::optional<native_block_t> block = on_block_result(buff);
            if (block) {
               block->syncing = true;
               blocks_channel.publish(80, std::make_shared<channels::native_block>(std::move(*block)));
            }
            read_next_block();
         });
      }

      void sync() {
         const auto& res = get_block_result(0, 0);
         uint32_t core_head = res.head.block_num;
         SILK_INFO << "Syncing from block #" << genesis << " and stay up to date";
         last_received = genesis;
         get_blocks_request(genesis, 0xffffffff);
         read_next_block();
      }

   private:
      std::shared_ptr<tcp::resolver>                  resolver;
      std::shared_ptr<websocket::stream<tcp::socket>> stream;
      constexpr static uint32_t                       priority = 40;
      std::string                                     host;
      std::string                                     port;
      abieos::abi                                     abi;
      uint32_t                                        received_blocks = 0;
      uint32_t                                        last_received   = 0;
      channels::native_blocks::channel_type&          blocks_channel;
      uint32_t                                        genesis;
      eosio::name                                     core_account;
      uint32_t                                        latest_height = 0;
      chain_state                                     core_chain_state;
};

ship_receiver_plugin::ship_receiver_plugin() : my(new ship_receiver_plugin_impl) {}
ship_receiver_plugin::~ship_receiver_plugin() {}

void ship_receiver_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("ship-endpoint", boost::program_options::value<std::string>()->default_value("127.0.0.1:8999"),
        "SHiP host address")
      ("ship-genesis", boost::program_options::value<uint32_t>()->default_value(0),
        "SHiP genesis block to start at")
      ("ship-core-account", boost::program_options::value<std::string>()->default_value("evmevmevmevm"),
        "Account on the core blockchain that hosts the Trust EVM runtime")
      ("ship-chain-state-dir", boost::program_options::value<std::string>()->default_value("./chaindata"),
        "Directory for SHiP chain state information")

   ;
}

void ship_receiver_plugin::plugin_initialize( const appbase::variables_map& options ) {
   auto endpoint = options.at("ship-endpoint").as<std::string>();
   const auto& i = endpoint.find(":");
   auto genesis  = options.at("ship-genesis").as<uint32_t>();
   auto core     = options.at("ship-core-account").as<std::string>();
   auto cs_dir   = appbase::app().get_plugin<engine_plugin>().get_chain_data_dir();
   SILK_INFO << "CHAIN_DATA DIR" << cs_dir;
   my->init(endpoint.substr(0, i), endpoint.substr(i+1), genesis, eosio::name(core), cs_dir);
   SILK_INFO << "Initialized SHiP Receiver Plugin";
}

void ship_receiver_plugin::plugin_startup() {
   SILK_INFO << "Started SHiP Receiver";
   my->initial_read();
   my->sync();
}

void ship_receiver_plugin::plugin_shutdown() {
   SILK_INFO << "Shutdown SHiP Receiver";
}

void ship_receiver_plugin::update_core_chain_state(uint32_t block_num) {
   my->update_core_chain_state(block_num);
}

void ship_receiver_plugin::update_trust_chain_state(uint32_t block_num) {
   my->update_trust_chain_state(block_num);
}

chain_state ship_receiver_plugin::get_chain_state() const {
   return my->get_chain_state();
}
