#pragma once

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key.hpp>

namespace mock_evm {

using namespace boost::multiprecision;

using uint160_t = number<cpp_int_backend<160, 160, unsigned_magnitude, unchecked, void>>;

struct staker {
   uint160_t address;
   uint256_t staked_wei;
   std::string url;
};
struct by_address {};

typedef boost::multi_index_container<
  staker,
  boost::multi_index::indexed_by<
    boost::multi_index::random_access<>,
    boost::multi_index::ordered_non_unique<boost::multi_index::tag<by_address>,boost::multi_index::key<&staker::address>>
  >
> staked_addresses_container;

struct mock_evm_session : public::std::enable_shared_from_this<mock_evm_session> {
   mock_evm_session(boost::asio::ip::tcp::socket&& sock, staked_addresses_container& state);

   void go();

private:
   void do_read();
   std::string do_eth_call(boost::json::value& params);

   boost::asio::ip::tcp::socket sock;
   boost::beast::flat_buffer buffer;
   boost::beast::http::request<boost::beast::http::string_body> req;
   std::unique_ptr<boost::beast::http::response<boost::beast::http::string_body>> res;

   staked_addresses_container& state;
};

class mock_evm_node {
public:
   mock_evm_node(boost::asio::io_context& ctx);

   void add_to_allow_list(const uint160_t& addr);
   void remove_from_allow_list(const uint160_t& addr);
   void set_stake_amount(const uint160_t& addr, const uint256_t& wei);
   void set_url(const uint160_t& addr, const std::string& url);

   std::string get_endpoint_string() const {
      std::stringstream ss;
      ss << acceptor.local_endpoint();
      return ss.str();
   }

private:
   void do_accept();

   boost::asio::io_context& ctx;
   boost::asio::ip::tcp::acceptor acceptor;

   staked_addresses_container staked_addresses;
};

}