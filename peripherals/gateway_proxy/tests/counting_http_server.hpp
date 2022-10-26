#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>

struct counting_http_server_session : public::std::enable_shared_from_this<counting_http_server_session> {
   counting_http_server_session(boost::asio::ip::tcp::socket&& sock, uint64_t& count);

   void go();

private:
   void do_read();

   boost::asio::ip::tcp::socket sock;
   boost::beast::flat_buffer buffer;
   boost::beast::http::request<boost::beast::http::string_body> req;
   boost::beast::http::response<boost::beast::http::string_body> res;

   uint64_t& count;
};

class counting_http_server {
public:
   counting_http_server(boost::asio::io_context& ctx, const uint16_t port);

   const uint64_t& get_count() const {return count;}
   void reset_count() { count = 0; }
   std::string get_endpoint_string() const {
      std::stringstream ss;
      ss << acceptor.local_endpoint();
      return ss.str();
   }
   std::string get_url_string() const {
      std::stringstream ss;
      ss << "https://" << acceptor.local_endpoint() << "/";
      return ss.str();
   }

private:
   void do_accept();

   boost::asio::io_context& ctx;
   boost::asio::ip::tcp::acceptor acceptor;

   uint64_t count = 0;
};