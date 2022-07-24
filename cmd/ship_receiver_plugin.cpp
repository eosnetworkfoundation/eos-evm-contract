#include "ship_receiver_plugin.hpp"
//#include "thread_pool.hpp"

#include <iostream>
#include <string>

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

#define CHECK_EC(ERR_MSG, ...)           \
   __VA_ARGS__;                          \
   if (ec)                               \
      throw std::runtime_error(ERR_MSG); \

class ship_receiver_plugin_impl : std::enable_shared_from_this<ship_receiver_plugin_impl> {
   public:
      ship_receiver_plugin_impl() {}

      void init(std::string h, std::string p) {
         host = h;
         port = p;
         resolver = std::make_shared<tcp::resolver>(appbase::app().get_io_service());
         stream   = std::make_shared<websocket::stream<tcp::socket>>(appbase::app().get_io_service());
         stream->binary(true);
         stream->read_message_max(0x1ull << 36);
      }

      void receive_abi(const flat_buffer& b) {
         std::string copy = {(const char*)b.data().data(), b.data().size()};
         std::string err = "";
         eosio::json_token_stream stream = {copy.data()};
         eosio::abi_def def = eosio::from_json<eosio::abi_def>(stream);
         eosio::convert(def, abi);
         SILK_DEBUG << "SHiP ABI Version " << def.version;
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
      }

      void get_blocks() {
         abieos::jarray positions = {};
         bool fetch_block = true;
         bool fetch_traces = false;
         bool fetch_deltas = true;

         send_request(
            abieos::jvalue{abieos::jarray{{"get_blocks_request_v0"},
               {abieos::jobject{
                  {{"start_block_num"}, {std::to_string(1)}},
                  {{"end_block_num"}, {std::to_string(2)}},
                  {{"max_messages_in_flight"}, {std::to_string(1000)}},
                  {{"have_positions"}, {positions}},
                  {{"irreversible_only"}, {false}},
                  {{"fetch_block"}, {fetch_block}},
                  {{"fetch_traces"}, {fetch_traces}},
                  {{"fetch_deltas"}, {fetch_deltas}},
               }}}});
      }

      void initial_read() {
         flat_buffer read_buff;
         boost::system::error_code ec;
         stream->read(read_buff, ec);
         if (ec) {
            SILK_CRIT << "SHiP initial read failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }
         receive_abi(read_buff);
      }

      void check_variant(eosio::input_stream& bin, const eosio::abi_type& type, const char* expected) const {
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
      }

      bool receive_result(flat_buffer& b) {
         eosio::input_stream bin = {(const char*)b.data(), (const char*)b.data() + b.size()};
         check_variant(bin, get_type("result"), "get_blocks_result_v0");
      }

      void read() {
         flat_buffer read_buff;
         boost::system::error_code ec;
         stream->read(read_buff, ec);
         if (ec) {
            SILK_CRIT << "SHiP read failed : " << ec.message();
            throw std::runtime_error(ec.message());
         }

         receive_result(read_buff);
      }

   private:
      std::shared_ptr<tcp::resolver>                  resolver;
      std::shared_ptr<websocket::stream<tcp::socket>> stream;
      constexpr static uint32_t                       priority = 40;
      std::string                                     host;
      std::string                                     port;
      abieos::abi                                     abi;
      uint32_t                                        received_blocks = 0;
};

ship_receiver_plugin::ship_receiver_plugin() : my(new ship_receiver_plugin_impl) {}
ship_receiver_plugin::~ship_receiver_plugin() {}

void ship_receiver_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("ship-host", boost::program_options::value<std::string>()->default_value("127.0.0.1"),
        "SHiP host address"),
      ("ship-port", boost::program_options::value<std::string>()->default_value("8888"),
        "SHiP port")
   ;
}

void ship_receiver_plugin::plugin_initialize( const appbase::variables_map& options ) {
   my->init("localhost", "8999");
   SILK_INFO << "Initialized SHiP Receiver Plugin";
}

void ship_receiver_plugin::plugin_startup() {
   my->connect_stream();
   my->initial_read();
   my->get_blocks();
   my->read();
}


void ship_receiver_plugin::plugin_shutdown() {

}