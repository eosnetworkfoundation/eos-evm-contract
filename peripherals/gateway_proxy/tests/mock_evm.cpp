#include "mock_evm.hpp"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <iostream>

namespace mock_evm {

using namespace boost::multiprecision;
using namespace boost::json;

mock_evm_session::mock_evm_session(boost::asio::ip::tcp::socket&& sock, staked_addresses_container& state) : sock(std::move(sock)), state(state) {}

void mock_evm_session::go() {
   do_read();
}

void mock_evm_session::do_read() {
   req = {};
   boost::beast::http::async_read(sock, buffer, req, [self = shared_from_this()](const boost::beast::error_code& ec, std::size_t) {
      if(ec)
         return;

      try {
         boost::json::value jreq = parse(self->req.body());
         const boost::json::string& method = jreq.at("method").as_string();

         boost::json::object jresp = {
                 {"jsonrpc", "2.0"},
                 {"id", jreq.at("id")}
         };

         if(method == "eth_blockNumber") {
            std::stringstream ss;
            ss << std::hex << std::showbase << std::setfill('0') << std::setw(8) << time(NULL);
            jresp["result"] = ss.str();
         }
         else if(method == "eth_call") {
            jresp["result"] = self->do_eth_call(jreq.at("params"));
         }

         self->res = std::make_unique<boost::beast::http::response<boost::beast::http::string_body>>(std::piecewise_construct,
                                                                                                     std::make_tuple(std::move(boost::json::serialize(jresp))),
                                                                                                     std::make_tuple(boost::beast::http::status::ok, self->req.version()));

         self->res->set(boost::beast::http::field::content_type, "application/json");
         self->res->keep_alive(self->req.keep_alive());
         self->res->prepare_payload();
         boost::beast::http::async_write(self->sock, *self->res, [self = self->shared_from_this()](const boost::beast::error_code& ec, std::size_t) {
            if(ec || self->res->need_eof())
               return;
            self->res.reset();
            self->do_read();
         });
      }
      catch(std::exception& e) {
         std::cerr << "exception parsing request " << e.what() << std::endl;
      }
   });
}

std::string mock_evm_session::do_eth_call(boost::json::value& params) {
   string& eth_call_data = params.at_pointer("/0/data").as_string();
   std::string_view method = eth_call_data.subview(2, 8);

   std::stringstream ss;
   ss << "0x";

   if(method == "3780b3ed") { //STAKER_ROLE() -> bytes32
      ss << std::hex << std::noshowbase << std::setfill('0') << std::setw(64) << uint256_t(0x01020304U);
   }
   if(method == "ca15c873") { //getRoleMemberCount(bytes32) -> uint256
      ss << std::hex << std::noshowbase << std::setfill('0') << std::setw(64) << uint256_t(state.size());
   }
   else if(method == "9010d07c") { //getRoleMember(bytes32, uint256) -> address
      uint256_t i;
      //std::string_view role = eth_call_data.subview(10, 64);
      std::string_view index = eth_call_data.subview(74, 64);
      boost::iostreams::stream<boost::iostreams::basic_array_source<char>> stream(index.begin(), index.size());

      stream >> std::hex >> i;

      ss << std::hex << std::noshowbase << std::setfill('0') << std::setw(64) << state.at(i.convert_to<uint64_t>()).address;
   }
   else if(method == "f5a79767") { //getAmount(address) -> uint256
      std::string_view address_hex = eth_call_data.subview(10, 64);
      boost::iostreams::stream<boost::iostreams::basic_array_source<char>> stream(address_hex.begin(), address_hex.size());
      uint160_t address;
      stream >> std::hex >> address;

      auto it = state.get<by_address>().find(address);
      if(it == state.get<by_address>().end())
         throw std::runtime_error("unknown address");

      ss << std::hex << std::noshowbase << std::setfill('0') << std::setw(64) << it->staked_wei;
   }
   else if(method == "2bdbee58") { //getUpstreamUrl(address) -> string
      std::string_view address_hex = eth_call_data.subview(10, 64);
      boost::iostreams::stream<boost::iostreams::basic_array_source<char>> stream(address_hex.begin(), address_hex.size());
      uint160_t address;
      stream >> std::hex >> address;

      auto it = state.get<by_address>().find(address);
      if(it == state.get<by_address>().end())
         throw std::runtime_error("unknown address");

      ss << std::hex << std::noshowbase << std::setfill('0') << std::setw(64) << uint256_t(32U);
      ss << std::hex << std::noshowbase << std::setfill('0') << std::setw(64) << uint256_t(it->url.size());

      for(unsigned i = 0; i < it->url.size(); i++)
         ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)(unsigned char)it->url[i];
      unsigned pad = it->url.size()%32;
      while(pad++%32)
         ss << "00";
   }

   return ss.str();
}

mock_evm_node::mock_evm_node(boost::asio::io_context& ctx) : ctx(ctx), acceptor(ctx, {boost::asio::ip::make_address("127.0.0.1"), 9999U}) {
   do_accept();
}

void mock_evm_node::add_to_allow_list(const uint160_t& addr) {
   staked_addresses.push_back({addr, uint256_t{0}});
}

void mock_evm_node::remove_from_allow_list(const uint160_t& addr) {
   auto it = staked_addresses.get<by_address>().find(addr);
   if(it == staked_addresses.get<by_address>().end())
      throw std::runtime_error("removing an address that isn't in the allow list");
   staked_addresses.get<by_address>().modify(it, [](staker& s) {
      s.address = 0;
   });
}

void mock_evm_node::set_stake_amount(const uint160_t& addr, const uint256_t& wei) {
   auto it = staked_addresses.get<by_address>().find(addr);
   if(it == staked_addresses.get<by_address>().end())
      throw std::runtime_error("setting staked amount for address not in allow list");
   staked_addresses.get<by_address>().modify(it, [&wei](staker& s) {
      s.staked_wei = wei;
   });
}

void mock_evm_node::set_url(const uint160_t& addr, const std::string& url) {
   auto it = staked_addresses.get<by_address>().find(addr);
   if(it == staked_addresses.get<by_address>().end())
      throw std::runtime_error("setting staked url for address not in allow list");
   staked_addresses.get<by_address>().modify(it, [&url](staker& s) {
      s.url = url;
   });
}

void mock_evm_node::do_accept() {
   acceptor.async_accept([&](const boost::system::error_code& ec, boost::asio::ip::tcp::socket sock) {
      if(ec)
         return;

      std::make_shared<mock_evm_session>(std::move(sock), staked_addresses)->go();

      do_accept();
   });
}

}