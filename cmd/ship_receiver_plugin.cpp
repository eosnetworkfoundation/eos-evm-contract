#include "ship_receiver_plugin.hpp"
//#include "thread_pool.hpp"

#include <atomic>
#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>

#include <eosio/abi.hpp>
#include <abieos.hpp>

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

class ship_receiver_plugin_impl : std::enable_shared_from_this<ship_receiver_plugin_impl> {
   public:
      ship_receiver_plugin_impl()
         : blocks_channel(appbase::app().get_channel<channels::blocks>()),
           sigs(appbase::app().get_io_service()) {

            SILK_DEBUG << "Signals registered on scheduler";
            const auto& handler = [this](const boost::system::error_code& ec, int sig) {
               SILK_DEBUG << "Signal caught " << sig;
               this->shutdown();
            };
            sigs.async_wait(handler);
         }

      using block_range_t  = std::pair<std::uint32_t, std::uint32_t>;
      using block_result_t = eosio::ship_protocol::get_blocks_result_v0;
      using native_block_t = channels::native_block;

      void init(std::string h, std::string p, uint32_t g, eosio::name ca) {
         host = h;
         port = p;
         genesis = g;
         core_account = ca;
         resolver = std::make_shared<tcp::resolver>(appbase::app().get_io_service());
         stream   = std::make_shared<websocket::stream<tcp::socket>>(appbase::app().get_io_service());
         stream->binary(true);
         stream->read_message_max(0x1ull << 36);
         connect_stream();
         shutdown_subscription = appbase::app().get_channel<channels::shutdown_signal>().subscribe(
            [this](auto s) {
               this->shutdown();
            }
         );

      }

      void initial_read() {
         flat_buffer buff;
         boost::system::error_code ec;
         stream->read(buff, ec);
         if (ec) {
            SILK_CRIT << "SHiP initial read failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }

         eosio::json_token_stream stream = {(char*)buff.data().data()};
         eosio::abi_def def = eosio::from_json<eosio::abi_def>(stream);
         SILK_DEBUG << "SHiP ABI Version " << def.version;
         eosio::convert(def, abi);
      }
      
      const auto& get_type(const std::string& n) {
         auto it = abi.abi_types.find(n);
         if (it == abi.abi_types.end())
            throw std::runtime_error("unknown ABI type <"+n+">");
         return it->second;
      }

      void send_request(const abieos::jvalue& v) {
         std::vector<char> bin;
         try {
            abieos::json_to_bin(bin, &get_type("request"), v, [&](){});
         } catch (std::runtime_error err) {
            SILK_CRIT << err.what();
            throw err;
         }
         boost::system::error_code ec;
         stream->write(asio::buffer(bin), ec);
         if (ec) {
            SILK_CRIT << "Sending request failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }
      }

      void connect_stream() {
         boost::system::error_code ec;
         auto res = resolver->resolve( tcp::v4(), host, port, ec);
         if (ec) {
            SILK_CRIT << "Resolver failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }

         boost::asio::connect(stream->next_layer(), res, ec);

         if (ec) {
            SILK_CRIT << "SHiP connection failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }

         stream->handshake(host, "/", ec);

         if (ec) {
            SILK_CRIT << "SHiP failed handshake : " << ec.message();
            throw std::runtime_error(ec.message());
         }

         SILK_INFO << "Connected to SHiP at " << host << ":" << port;
      }

      inline auto read() const {
         auto buff = std::make_shared<flat_buffer>();
         boost::system::error_code ec;
         stream->read(*buff, ec);
         if (ec) {
            SILK_CRIT << "SHiP read failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }
         return buff;
      }

      void get_blocks_request(std::size_t start, std::size_t end, bool deltas = false) {
         abieos::jarray positions = {};
         bool fetch_block  = true;
         bool fetch_traces = true;
         bool fetch_deltas = deltas;

         send_request(
            abieos::jvalue{abieos::jarray{{"get_blocks_request_v0"},
               {abieos::jobject{
                  {{"start_block_num"}, {std::to_string(start)}},
                  {{"end_block_num"}, {std::to_string(end)}},
                  {{"max_messages_in_flight"}, {std::to_string(4*1024)}},
                  {{"have_positions"}, {positions}},
                  {{"irreversible_only"}, {false}},
                  {{"fetch_block"}, {fetch_block}},
                  {{"fetch_traces"}, {fetch_traces}},
                  {{"fetch_deltas"}, {fetch_deltas}},
               }}}});
      }

      void get_blocks_request() {
         get_blocks_request(received_blocks, received_blocks+1, false);
      } 

      void get_deltas_request() {
         get_blocks_request(received_blocks, received_blocks+1, true);
      }

      template <typename Buffer>
      eosio::ship_protocol::get_blocks_result_v0 get_result(Buffer&& b){
         auto data = b->data();
         eosio::input_stream bin = {(const char*)data.data(), (const char*)data.data() + data.size()};
         verify_variant(bin, get_type("result"), "get_blocks_result_v0");

         auto block = eosio::from_bin<eosio::ship_protocol::get_blocks_result_v0>(bin);
         return block;
      }

      template <typename Buffer>
      std::optional<native_block_t> on_block_result(Buffer&& b) {
         native_block_t current_block;
         auto block = get_result(std::forward<Buffer>(b));

         if (!block.this_block || block.this_block->block_num <= latest_height) {
            return std::nullopt;
         }

         latest_height = block.this_block->block_num;

         received_blocks++;

         uint32_t new_lib = block.last_irreversible.block_num;
         if (new_lib > lib) {
            SILK_DEBUG << "LIB advanced from " << lib << " to " << new_lib;
            lib = new_lib;
         }

         if (block.traces) {
            uint32_t num;
            eosio::varuint32_from_bin(num, *block.traces);
            //SILK_DEBUG << "Block #" << ptr->block_num << " with " << num << " transactions and produced by : " << sb.producer.to_string();
            for (std::size_t i = 0; i < num; i++) {
               auto tt = eosio::from_bin<eosio::ship_protocol::transaction_trace>(*block.traces);
               const auto& trace = std::get<eosio::ship_protocol::transaction_trace_v0>(tt);
               const auto& actions = trace.action_traces;
               for (std::size_t j=0; j < actions.size(); j++) {
                  const auto& act = std::get<eosio::ship_protocol::action_trace_v1>(actions[j]);
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

         return current_block.building ? std::optional<native_block_t>(std::move(current_block)) : std::nullopt;
      }

      template <typename BlockResult>
      inline native_block_t start_native_block(BlockResult&& res) const {
         native_block_t block;
         block.block_num = res.this_block->block_num;
         eosio::ship_protocol::signed_block sb;
         eosio::from_bin(sb, *res.block);
         block.timestamp = sb.timestamp.to_time_point().time_since_epoch().count();
         SILK_INFO << "Started native block " << block.block_num;
         return block;
      }

      template <typename Trx>
      inline native_block_t& append_to_block(native_block_t& block, Trx&& trx) {
         channels::native_trx native_trx = {trx.id, trx.cpu_usage_us, trx.elapsed};
         const auto& actions = trx.action_traces;
         SILK_INFO  << "Appending transaction ";
         for (std::size_t j=0; j < actions.size(); j++) {
            const auto& act = std::get<eosio::ship_protocol::action_trace_v1>(actions[j]);
            channels::native_action action = {
               act.action_ordinal,
               act.receiver,
               act.act.account,
               act.act.name,
               std::move(act.act.data)
            };
            native_trx.actions.emplace_back(std::move(action));
            SILK_INFO  << "Appending action " << native_trx.actions.back().name.to_string();
         }
         return block;
      }

     
      eosio::ship_protocol::get_blocks_result_v0 get_block_result(std::size_t s, std::size_t e){
         get_blocks_request(s, e, false);
         return get_result(read());
      }

      std::optional<native_block_t> get_block(std::size_t s, std::size_t e){
         get_blocks_request(s, e, false);
         return on_block_result(read());
      }

      void verify_variant(eosio::input_stream& bin, const eosio::abi_type& type, const char* expected) const {
         uint32_t index;
         eosio::varuint32_from_bin(index, bin);
         auto v = type.as_variant();

         if (!v) {
            SILK_CRIT << type.name << " not a variant";
            throw std::runtime_error("not a variant");
         }
         if (index >= v->size()) {
            SILK_CRIT << "Expected " << expected << " got " << std::to_string(index);
            throw std::runtime_error("not expected");
         }
         if (v->at(index).name != expected) {
            SILK_CRIT << "Expected " << expected << " got " << v->at(index).name;
            throw std::runtime_error("not expected");
         }

         SILK_DEBUG << "check_variant successful <variant = " << v->at(index).name << ">";
      }

      void shutdown() {
         SILK_INFO << "Should shutdown";
         should_shutdown.store(false);
      }

      bool sync() {
         // get the block range we can grab
         const auto& res = get_block_result(0, 0);
         uint32_t core_head = res.head.block_num;
         SILK_INFO << "Syncing from block #" << genesis << " to block #" << core_head;
         last_received = genesis;
         if (genesis <= core_head) {
            for (std::size_t i=0; i < core_head - genesis; i++) {
               auto block_ptr = get_block(genesis+i, core_head);
            }
         }
         SILK_INFO << "Finished Syncing";
         return true;
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
      uint32_t                                        lib = 0;
      uint32_t                                        head = 0; // TODO fix this to point to head for silkworm chain
      channels::blocks::channel_type&                 blocks_channel;
      block_range_t                                   sync_range;
      uint32_t                                        genesis;
      eosio::name                                     core_account;
      channels::shutdown_signal::channel_type::handle shutdown_subscription;
      std::atomic<bool>                               should_shutdown = false;
      boost::asio::signal_set                         sigs;
      uint32_t                                        latest_height;
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
   ;
}

void ship_receiver_plugin::plugin_initialize( const appbase::variables_map& options ) {
   auto endpoint = options.at("ship-endpoint").as<std::string>();
   const auto& i = endpoint.find(":");
   auto genesis  = options.at("ship-genesis").as<uint32_t>();
   auto core     = options.at("ship-core-account").as<std::string>();
   my->init(endpoint.substr(0, i), endpoint.substr(i+1), genesis, eosio::name(core));
   SILK_INFO << "Initialized SHiP Receiver Plugin";
}

void ship_receiver_plugin::plugin_startup() {
   SILK_INFO << "Started SHiP Receiver";
   my->initial_read();
   my->sync();
   //my->get_blocks();
   //my->read();
}

void ship_receiver_plugin::plugin_shutdown() {

}