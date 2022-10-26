#include "http_cannon.hpp"
#include <boost/beast.hpp>

boost::asio::awaitable<void> launch_cannon(unsigned count, uint16_t port) {
   for(unsigned i = 0 ; i < count; ++i) {
      boost::asio::ip::tcp::socket sock(co_await boost::asio::this_coro::executor);
      co_await sock.async_connect({boost::asio::ip::make_address("127.0.0.1"), port}, boost::asio::use_awaitable);

      boost::beast::http::request<boost::beast::http::string_body> req(boost::beast::http::verb::get, "/", 11);
      req.set(boost::beast::http::field::host, "127.0.0.1");
      co_await boost::beast::http::async_write(sock, req, boost::asio::use_awaitable);

      boost::beast::flat_buffer buffer;
      boost::beast::http::response<boost::beast::http::string_body> res;
      co_await boost::beast::http::async_read(sock, buffer, res, boost::asio::use_awaitable);

      if(res.result() != boost::beast::http::status::ok)
         throw std::runtime_error("hm didn't get 200 from server");
   }
}
