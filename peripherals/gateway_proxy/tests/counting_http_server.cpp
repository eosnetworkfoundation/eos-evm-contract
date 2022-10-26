#include "counting_http_server.hpp"
#include <iostream>

counting_http_server::counting_http_server(boost::asio::io_context& ctx, const uint16_t port) : ctx(ctx), acceptor(ctx, {boost::asio::ip::make_address("127.0.0.1"), port}) {
   do_accept();
}

void counting_http_server::do_accept() {
   acceptor.async_accept([&](const boost::system::error_code& ec, boost::asio::ip::tcp::socket sock) {
      if(ec)
         return;

      std::make_shared<counting_http_server_session>(std::move(sock), count)->go();

      do_accept();
   });
}

counting_http_server_session::counting_http_server_session(boost::asio::ip::tcp::socket&& sock, uint64_t& count) : sock(std::move(sock)), count(count) {}

void counting_http_server_session::go() {
   do_read();
}

void counting_http_server_session::do_read() {
   req = {};
   boost::beast::http::async_read(sock, buffer, req, [self = shared_from_this()](const boost::beast::error_code& ec, std::size_t) {
      if(ec)
         return;

      self->count++;

      self->res = boost::beast::http::response<boost::beast::http::string_body>(std::piecewise_construct,
                                                                               std::make_tuple("OK!"),
                                                                               std::make_tuple(boost::beast::http::status::ok, self->req.version()));

      self->res.keep_alive(self->req.keep_alive());
      self->res.prepare_payload();
      boost::beast::http::async_write(self->sock, self->res, [self = self->shared_from_this()](const boost::beast::error_code& ec, std::size_t) {
         if(ec || self->res.need_eof())
            return;
         self->do_read();
      });
   });
}