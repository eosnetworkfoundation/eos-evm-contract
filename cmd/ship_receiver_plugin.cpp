#include "ship_receiver_plugin.hpp"
#include "abi_utils.hpp"
#include "utils.hpp"

#include <string>
#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/program_options.hpp>

namespace asio      = boost::asio;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

using sys = sys_plugin;
class ship_receiver_plugin_impl : std::enable_shared_from_this<ship_receiver_plugin_impl> {
   public:
      ship_receiver_plugin_impl()
         : native_blocks_channel(appbase::app().get_channel<channels::native_blocks>()) {
         }

      using block_result_t = eosio::ship_protocol::get_blocks_result_v0;
      using native_block_t = channels::native_block;
      static constexpr eosio::name pushtx = eosio::name("pushtx");

      void init(std::string h, std::string p, eosio::name ca,
                std::optional<eosio::checksum256> start_block_id, int64_t start_block_timestamp,
                uint32_t input_max_retry, uint32_t input_delay_second) {
         SILK_DEBUG << "ship_receiver_plugin_impl INIT";
         host = std::move(h);
         port = std::move(p);
         core_account = ca;
         start_from_block_id = start_block_id;
         start_from_block_timestamp = start_block_timestamp;
         last_lib = 0;
         last_block_num = 0;
         delay_second = input_delay_second;
         max_retry = input_max_retry;
         retry_count = 0;
         resolver = std::make_shared<tcp::resolver>(appbase::app().get_io_service());
         // Defer connection to plugin_start()
      }

      void shutdown() {
      }

      auto send_request(const eosio::ship_protocol::request& req) {
         auto bin = eosio::convert_to_bin(req);
         boost::system::error_code ec;
         stream->write(asio::buffer(bin), ec);
         if (ec) {
            SILK_ERROR << "Sending request failed : " << ec.message();
         }
         return ec;
      }

      auto connect_stream(auto& resolve_it) {
         boost::system::error_code ec;
         boost::asio::connect(stream->next_layer(), resolve_it, ec);

         if (ec) {
            SILK_ERROR << "SHiP connection failed : " << ec.message();
            return ec;
         }

         stream->handshake(host, "/", ec);

         if (ec) {
            SILK_ERROR << "SHiP failed handshake : " << ec.message();
            return ec;
         }

         SILK_INFO << "Connected to SHiP at " << host << ":" << port;
         return ec;
      }

      auto initial_read() {
         flat_buffer buff;
         boost::system::error_code ec;
         stream->read(buff, ec);
         if (ec) {
            SILK_ERROR << "SHiP initial read failed : " << ec.message();
         }
         auto end = buff.prepare(1);
         ((char *)end.data())[0] = '\0';
         buff.commit(1);
         abi = load_abi(eosio::json_token_stream{(char *)buff.data().data()});
         return ec;
      }

      inline auto read(flat_buffer& buff) const {
         boost::system::error_code ec;
         stream->read(buff, ec);
         if (ec) {
            SILK_ERROR << "SHiP read failed : " << ec.message();
         }
         return ec;
      }

      template <typename F>
      inline void async_read(F&& func) const {
         auto buff = std::make_shared<flat_buffer>();

         stream->async_read(*buff, appbase::app().get_priority_queue().wrap(80,
            [buff, func](const auto ec, auto) {
               if (ec) {
                  SILK_ERROR << "SHiP read failed : " << ec.message();
               } 
               func(ec, buff);
            })
         );
      }

      auto send_get_blocks_request(uint32_t start) {
         eosio::ship_protocol::request req = eosio::ship_protocol::get_blocks_request_v0{
            .start_block_num        = start,
            .end_block_num          = std::numeric_limits<uint32_t>::max(),
            .max_messages_in_flight = 4*1024,
            .have_positions         = {},
            .irreversible_only      = false,
            .fetch_block            = true,
            .fetch_traces           = true,
            .fetch_deltas           = false
         };
         return send_request(req);
      }

      auto send_get_status_request() {
         eosio::ship_protocol::request req = eosio::ship_protocol::get_status_request_v0{};
         return send_request(req);
      }

      auto send_get_blocks_ack_request(uint32_t num_messages) {
         eosio::ship_protocol::request req = eosio::ship_protocol::get_blocks_ack_request_v0{num_messages};
         return send_request(req);
      }

      template <typename Buffer>
      eosio::ship_protocol::result get_result(Buffer&& b){
         auto data = b->data();
         eosio::input_stream bin = {(const char*)data.data(), (const char*)data.data() + data.size()};
         return eosio::from_bin<eosio::ship_protocol::result>(bin);
      }

      template <typename T>
      std::optional<native_block_t> to_native(T&& block) {

         if (!block.this_block) {
           //TODO: throw here?
           return std::nullopt;
         }

         auto current_block = start_native_block(block);

         if (block.traces) {
            uint32_t num;
            eosio::varuint32_from_bin(num, *block.traces);
            //SILK_DEBUG << "Block #" << block.this_block->block_num << " with " << num << " transactions";
            for (std::size_t i = 0; i < num; i++) {
               auto tt = eosio::from_bin<eosio::ship_protocol::transaction_trace>(*block.traces);
               const auto& trace = std::get<eosio::ship_protocol::transaction_trace_v0>(tt);
               const auto& actions = trace.action_traces;
               bool found = false;
               for (std::size_t j=0; j < actions.size(); j++) {
                  std::visit([&](auto& act){
                     if (act.act.name == pushtx && core_account == act.receiver) {
                        append_to_block(current_block, trace, j);
                        found = true;
                     }
                  }, actions[j]);
                  if (found) break;
               }
            }
         }

         current_block.lib = block.last_irreversible.block_num;
         return std::optional<native_block_t>(std::move(current_block));
      }

      template <typename BlockResult>
      inline native_block_t start_native_block(BlockResult&& res) const {
         native_block_t block;
         eosio::ship_protocol::signed_block sb;
         eosio::from_bin(sb, *res.block);

         block.block_num = res.this_block->block_num;
         block.id        = res.this_block->block_id;
         block.prev      = res.prev_block->block_id;
         block.timestamp = sb.timestamp.to_time_point().time_since_epoch().count();

         //SILK_INFO << "Started native block " << block.block_num;
         return block;
      }

      template <typename Trace>
      inline void append_to_block(native_block_t& block, Trace& trace, std::size_t idx) {
         channels::native_trx native_trx = {trace.id, trace.cpu_usage_us, trace.elapsed};
         const auto& actions = trace.action_traces;
         //SILK_DEBUG << "Appending transaction ";
         for (std::size_t j=idx; j < actions.size(); j++) {
            std::visit([&](auto& act){
               if (act.act.name == pushtx && core_account == act.receiver) {
                   channels::native_action action = {
                       act.action_ordinal,
                       act.receiver,
                       act.act.account,
                       act.act.name,
                       std::vector<char>(act.act.data.pos, act.act.data.end)
                   };
                   native_trx.actions.emplace_back(std::move(action));
               }
            }, actions[j]);
            //SILK_DEBUG << "Appending action " << native_trx.actions.back().name.to_string();
         }
         block.transactions.emplace_back(std::move(native_trx));
      }
     
      auto get_status(auto& r){
         auto ec = send_get_status_request();
         if (ec) {
            return ec;
         }
         auto buff = std::make_shared<flat_buffer>();
         ec = read(*buff);
         if (ec) {
            return ec;
         }
         r = std::get<eosio::ship_protocol::get_status_result_v0>(get_result(buff));
         return ec;
      }

      void reset_connection() {
         // De facto entry point.
         if (stream) {
            // Try close connection gracefully but ignore return value.
            boost::system::error_code ec;
            stream->close(websocket::close_reason(websocket::close_code::normal),ec);

            // Determine if we should re-connect.
            if (++retry_count > max_retry) {
               // No more retry;
               sys::error("Max retry reached. No more reconnections.");
               return;
            }

            // Delay in the case of reconnection.
            std::this_thread::sleep_for(std::chrono::seconds(delay_second));
            SILK_INFO << "Trying to reconnect "<< retry_count << "/" << max_retry;
         }
         stream = std::make_shared<websocket::stream<tcp::socket>>(appbase::app().get_io_service());
         stream->binary(true);
         stream->read_message_max(0x1ull << 36);

         // CAUTION: we have to use async call here to avoid recursive reset_connection() calls.
         resolver->async_resolve( tcp::v4(), host, port, [this](const auto ec, auto res) {
            if (ec) {
               SILK_ERROR << "Resolver failed : " << ec.message();
               reset_connection();
               return;
            }

            // It should be fine to call connection and initial read synchronously as though they are 
            // blocking calls, it's only one thread and we have nothing more important to run anyway.
            auto ec2 = connect_stream(res);
            if (ec2) {
               reset_connection();
               return;
            }

            ec2 = initial_read();
            if (ec2) {
               reset_connection();
               return;
            }
            
            // Will call reset connection if necessary internally.
            sync();
         });         
      }

      void start_read() {
         static size_t num_messages = 0;
         async_read([this](const auto ec, auto buff) {
            if (ec) {
               SILK_INFO << "Trying to recover from SHiP read failure.";
               // Reconnect and restart sync.
               num_messages = 0;
               reset_connection();
               return;
            }
            auto block = to_native(std::get<eosio::ship_protocol::get_blocks_result_v0>(get_result(buff)));
            if(!block) {
               sys::error("Unable to generate native block");
               // No reset!
               return;
            }

            last_lib = block->lib;
            last_block_num = block->block_num;
            // reset retry_count upon successful read.
            retry_count = 0;

            native_blocks_channel.publish(80, std::make_shared<channels::native_block>(std::move(*block)));

            if(++num_messages % 1024 == 0) {
               //SILK_INFO << "Block #" << block->block_num;
               auto ec = send_get_blocks_ack_request(num_messages);
               if (ec) {
                  num_messages = 0;
                  reset_connection();
                  return;
               }
            }

            start_read();
         });
      }

      void sync() {
         SILK_INFO << "Start Syncing blocks.";
         // get available blocks range we can grab
         eosio::ship_protocol::get_status_result_v0 res = {};
         auto ec = get_status(res);
         if (ec) {
            reset_connection();
            return;
         }

         uint32_t start_from = 0;

         if (last_lib > 0) {
            // None zero last_lib means we are in the process of reconnection.
            // If last pushed block number is higher than LIB, we have the risk of fork and need to start from LIB.
            // Otherwise it means we are catching up blocks and can safely continue from next block.
            start_from = (last_lib > last_block_num ? last_block_num : last_lib) + 1;
            SILK_INFO << "Recover from disconnection, " << "last LIB is: " << last_lib
                     << ", last block num is: " << last_block_num
                     << ", continue from: " << start_from;
         }
         else {
            auto head_header = appbase::app().get_plugin<engine_plugin>().get_head_canonical_header();
            if (!head_header) {
               sys::error("Unable to read canonical header");
               // No reset!
               return;
            }

            // Only take care of canonical header and input options when it's initial sync.
            SILK_INFO << "Get_head_canonical_header: "
                     << "#" << head_header->number
                     << ", hash:" << silkworm::to_hex(head_header->hash())
                     << ", mixHash:" << silkworm::to_hex(head_header->mix_hash);

            start_from = utils::to_block_num(head_header->mix_hash.bytes) + 1;
            SILK_INFO << "Canonical header start from block: " << start_from;
            
            if( start_from_block_id ) {
               uint32_t block_num = utils::to_block_num(*start_from_block_id);
               SILK_INFO << "Using specified start block number:" << block_num;
               start_from = block_num;
            }
         }
         
         if( res.trace_begin_block > start_from ) {
            SILK_ERROR << "Block #" << start_from << " not available in SHiP";
            sys::error("Start block not available in SHiP");
            // No reset!
            return;
         }

         SILK_INFO << "Starting from block #" << start_from;
         ec = send_get_blocks_request(start_from);
         if (ec) {
            reset_connection();
            return;
         }
         start_read();
      }

      std::optional<channels::native_block> get_start_from_block() {
         if( !start_from_block_id ) return {};

         channels::native_block nb{};
         nb.id = *start_from_block_id;
         nb.timestamp = start_from_block_timestamp;
         nb.block_num = utils::to_block_num(*start_from_block_id);
         return nb;
      }

   private:
      std::shared_ptr<tcp::resolver>                  resolver;
      std::shared_ptr<websocket::stream<tcp::socket>> stream;
      constexpr static uint32_t                       priority = 40;
      std::string                                     host;
      std::string                                     port;
      abieos::abi                                     abi;
      channels::native_blocks::channel_type&          native_blocks_channel;
      eosio::name                                     core_account;
      std::optional<eosio::checksum256>               start_from_block_id;
      int64_t                                         start_from_block_timestamp{};
      uint32_t                                        last_lib;
      uint32_t                                        last_block_num;
      uint32_t                                        delay_second;
      uint32_t                                        max_retry;
      uint32_t                                        retry_count;
};

ship_receiver_plugin::ship_receiver_plugin() : my(new ship_receiver_plugin_impl) {}
ship_receiver_plugin::~ship_receiver_plugin() = default;

void ship_receiver_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
   cfg.add_options()
      ("ship-endpoint", boost::program_options::value<std::string>()->default_value("127.0.0.1:8999"),
        "SHiP host address")
      ("ship-core-account", boost::program_options::value<std::string>()->default_value("evmevmevmevm"),
        "Account on the core blockchain that hosts the EOS EVM Contract")
      ("ship-start-from-block-id", boost::program_options::value<std::string>(),
        "Override Antelope block id to start syncing from"  )
      ("ship-start-from-block-timestamp", boost::program_options::value<int64_t>(),
        "Timestamp for the provided ship-start-from-block-id, required if block-id provided"  )
      ("ship-max-retry", boost::program_options::value<uint32_t>(),
        "Max retry times before give up when trying to reconnect to SHiP endpoints"  )
      ("ship-delay-second", boost::program_options::value<uint32_t>(),
        "Deply in seconds between each retry when trying to reconnect to SHiP endpoints"  )
   ;
}

void ship_receiver_plugin::plugin_initialize( const appbase::variables_map& options ) {
   auto endpoint = options.at("ship-endpoint").as<std::string>();
   const auto& i = endpoint.find(":");
   auto core     = options.at("ship-core-account").as<std::string>();
   std::optional<eosio::checksum256> start_block_id;
   int64_t start_block_timestamp = 0;
   uint32_t delay_second = 10;
   uint32_t max_retry = 0;
   if (options.contains("ship-start-from-block-id")) {
      if (!options.contains("ship-start-from-block-timestamp")) {
         throw std::runtime_error("ship-start-from-block-timestamp required if ship-start-from-block-id provided");
      }
      start_block_id = utils::checksum256_from_string( options.at("ship-start-from-block-id").as<std::string>() );
      start_block_timestamp = options.at("ship-start-from-block-timestamp").as<int64_t>();
      SILK_INFO << "String from block id: " << utils::to_string( *start_block_id ) << " block num: " << utils::to_block_num(*start_block_id);
   } else if ( options.contains("ship-start-from-block-timestamp") ) {
      throw std::runtime_error("ship-start-from-block-timestamp only valid if ship-start-from-block-id provided");
   }

   if (options.contains("ship-max-retry")) {
      max_retry = options.at("ship-max-retry").as<uint32_t>();
   }

   if (options.contains("ship-delay-second")) {
      delay_second = options.at("ship-delay-second").as<uint32_t>();
   }

   my->init(endpoint.substr(0, i), endpoint.substr(i+1), eosio::name(core), start_block_id, start_block_timestamp, max_retry, delay_second);
   SILK_INFO << "Initialized SHiP Receiver Plugin";
}

void ship_receiver_plugin::plugin_startup() {
   SILK_INFO << "Started SHiP Receiver";
   my->reset_connection();

}

void ship_receiver_plugin::plugin_shutdown() {
   SILK_INFO << "Shutdown SHiP Receiver";
}

std::optional<channels::native_block> ship_receiver_plugin::get_start_from_block() {
   return my->get_start_from_block();
}
