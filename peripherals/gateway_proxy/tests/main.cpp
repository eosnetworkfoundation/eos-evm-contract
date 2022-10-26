#define BOOST_TEST_MODULE gateway_proxy_tests

#include "mock_evm.hpp"
#include "nginx.hpp"
#include "counting_http_server.hpp"
#include "http_cannon.hpp"
#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

using namespace mock_evm;

BOOST_AUTO_TEST_CASE(basic_test) {
   boost::asio::io_context ctx;

   co_spawn(ctx, [&]() -> boost::asio::awaitable<void> {
      const unsigned target_num = 10000;

      mock_evm_node node(ctx);
      counting_http_server fallbackupstream(ctx, 6000U);
      nginx n(fallbackupstream.get_endpoint_string(), "http://" + node.get_endpoint_string());
      co_await n.start_or_restart();

      counting_http_server upstream1(ctx, 5000U);
      counting_http_server upstream2(ctx, 5001U);
      counting_http_server upstream3(ctx, 5002U);

      uint160_t addr1("0x111111");
      node.add_to_allow_list(addr1);
      node.set_stake_amount(addr1, 1'000'000'000'000'000'000ULL);
      node.set_url(addr1, upstream1.get_url_string());

      uint160_t addr2("0x222222");
      node.add_to_allow_list(addr2);
      node.set_stake_amount(addr2, 1'000'000'000'000'000'000ULL);
      node.set_url(addr2, upstream2.get_url_string());

      co_await n.wait();

      //start with node 1 & 2 half and half
      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(upstream1.get_count() >= target_num/2-1 && upstream1.get_count() <= target_num/2+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/2-1 && upstream2.get_count() <= target_num/2+1);
      upstream1.reset_count();
      upstream2.reset_count();

      //try adding a third node with double the stake amount
      uint160_t addr3("0x333333");
      node.add_to_allow_list(addr3);
      node.set_stake_amount(addr3, 2'000'000'000'000'000'000ULL);
      node.set_url(addr3, upstream3.get_url_string());

      co_await n.wait();
      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(upstream1.get_count() >= target_num/4-1 && upstream1.get_count() <= target_num/4+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/4-1 && upstream2.get_count() <= target_num/4+1);
      BOOST_REQUIRE(upstream3.get_count() >= target_num/2-1 && upstream3.get_count() <= target_num/2+1);
      upstream1.reset_count();
      upstream2.reset_count();
      upstream3.reset_count();

      //now 1 & 2 increase their stake
      node.set_stake_amount(addr1, 2'000'000'000'000'000'000ULL);
      node.set_stake_amount(addr2, 2'000'000'000'000'000'000ULL);

      co_await n.wait();
      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(upstream1.get_count() >= target_num/3-1 && upstream1.get_count() <= target_num/3+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/3-1 && upstream2.get_count() <= target_num/3+1);
      BOOST_REQUIRE(upstream3.get_count() >= target_num/3-1 && upstream3.get_count() <= target_num/3+1);
      upstream1.reset_count();
      upstream2.reset_count();
      upstream3.reset_count();

      //set 2's stake to less than szabo
      node.set_stake_amount(addr2, 999'999'999'999ULL);
      co_await n.wait();
      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(upstream1.get_count() >= target_num/2-1 && upstream1.get_count() <= target_num/2+1);
      BOOST_REQUIRE(upstream3.get_count() >= target_num/2-1 && upstream3.get_count() <= target_num/2+1);
      upstream1.reset_count();
      upstream2.reset_count();
      upstream3.reset_count();

      //unregister 1
      node.remove_from_allow_list(addr1);
      co_await n.wait();
      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE_EQUAL(upstream3.get_count(), target_num);

   }, [](std::exception_ptr e) {
      if(e)
         std::rethrow_exception(e);
   });

   ctx.run();
}

BOOST_AUTO_TEST_CASE(fallback_only) {
   boost::asio::io_context ctx;

   co_spawn(ctx, [&]() -> boost::asio::awaitable<void> {
      const unsigned target_num = 1000;

      mock_evm_node node(ctx);
      counting_http_server fallbackupstream(ctx, 6000U);
      nginx n(fallbackupstream.get_endpoint_string(), "http://" + node.get_endpoint_string());
      co_await n.start_or_restart();

      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(fallbackupstream.get_count() >= target_num-1 && fallbackupstream.get_count() <= target_num+1);

   }, [](std::exception_ptr e) {
      if(e)
         std::rethrow_exception(e);
   });

   ctx.run();
}

//this test will only be able to pass with root, CAP_NET_ADMIN, or by tweaking ip_unprivileged_port_start
BOOST_AUTO_TEST_CASE(missing_port) {
   boost::asio::io_context ctx;

   co_spawn(ctx, [&]() -> boost::asio::awaitable<void> {
      const unsigned target_num = 10000;

      mock_evm_node node(ctx);
      counting_http_server fallbackupstream(ctx, 6000U);
      nginx n(fallbackupstream.get_endpoint_string(), "http://" + node.get_endpoint_string());

      counting_http_server upstream1(ctx, 5000U);
      counting_http_server upstream2(ctx, 443U);

      uint160_t addr1("0x111111");
      node.add_to_allow_list(addr1);
      node.set_stake_amount(addr1, 1'000'000'000'000'000'000ULL);
      node.set_url(addr1, upstream1.get_url_string());

      uint160_t addr2("0x222222");
      node.add_to_allow_list(addr2);
      node.set_stake_amount(addr2, 1'000'000'000'000'000'000ULL);
      node.set_url(addr2, "https://127.0.0.1/");

      co_await n.start_or_restart();

      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(upstream1.get_count() >= target_num/2-1 && upstream1.get_count() <= target_num/2+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/2-1 && upstream2.get_count() <= target_num/2+1);
   }, [](std::exception_ptr e) {
      if(e)
         std::rethrow_exception(e);
   });

   ctx.run();
}

//this test will only be able to pass with the file limit increased -- try 2048
BOOST_AUTO_TEST_CASE(lots) {
   boost::asio::io_context ctx;

   co_spawn(ctx, [&]() -> boost::asio::awaitable<void> {
      const unsigned count = 1000;
      const unsigned target_num = 10000;

      mock_evm_node node(ctx);
      counting_http_server fallbackupstream(ctx, 6000U);
      nginx n(fallbackupstream.get_endpoint_string(), "http://" + node.get_endpoint_string());

      std::list<counting_http_server> servers;
      for(unsigned int i = 0; i < count; ++i) {
         servers.emplace_back(ctx, 7000+i);

         uint160_t addr(i+10);
         node.add_to_allow_list(addr);
         node.set_stake_amount(addr, 1'000'000'000'000'000'000ULL);
         node.set_url(addr, servers.back().get_url_string());
      }

      co_await n.start_or_restart();

      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(servers.front().get_count() >= target_num/count-1 && servers.front().get_count() <= target_num/count+1);
      BOOST_REQUIRE(servers.back().get_count() >= target_num/count-1 && servers.back().get_count() <= target_num/count+1);
   }, [](std::exception_ptr e) {
      if(e)
         std::rethrow_exception(e);
   });

   ctx.run();
}

BOOST_AUTO_TEST_CASE(whale) {
   boost::asio::io_context ctx;

   co_spawn(ctx, [&]() -> boost::asio::awaitable<void> {
      const unsigned target_num = 10000;

      mock_evm_node node(ctx);
      counting_http_server fallbackupstream(ctx, 6000U);
      nginx n(fallbackupstream.get_endpoint_string(), "http://" + node.get_endpoint_string());

      counting_http_server upstream1(ctx, 5000U);
      counting_http_server upstream2(ctx, 443U);

      uint160_t addr1("0x111111");
      node.add_to_allow_list(addr1);
      node.set_stake_amount(addr1,  uint256_t(1'000'000'000'000'000'000ULL)*1'000'000); //1 million tokens
      node.set_url(addr1, upstream1.get_url_string());

      uint160_t addr2("0x222222");
      node.add_to_allow_list(addr2);
      node.set_stake_amount(addr2, uint256_t(1'000'000'000'000'000'000ULL)*100'000'000); //100 million tokens
      node.set_url(addr2, upstream2.get_url_string());

      co_await n.start_or_restart();

      co_await launch_cannon(target_num, 8000);
      BOOST_REQUIRE(upstream1.get_count() >= target_num/100-1 && upstream1.get_count() <= target_num/100+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/100*99-1 && upstream2.get_count() <= target_num/100*99+1);
   }, [](std::exception_ptr e) {
      if(e)
         std::rethrow_exception(e);
   });

   ctx.run();
}

BOOST_AUTO_TEST_CASE(hostname_types) {
   boost::asio::io_context ctx;

   co_spawn(ctx, [&]() -> boost::asio::awaitable<void> {
      const unsigned target_num = 10000;

      mock_evm_node node(ctx);
      counting_http_server fallbackupstream(ctx, 6000U);
      nginx n(fallbackupstream.get_endpoint_string(), "http://" + node.get_endpoint_string());

      counting_http_server upstream1(ctx, 5000U);
      counting_http_server upstream2(ctx, 5001U);
      counting_http_server upstream3(ctx, 5002U);

      uint160_t addr1("0x111111");
      node.add_to_allow_list(addr1);
      node.set_stake_amount(addr1,  1'000'000'000'000'000'000ULL);
      node.set_url(addr1, "https://localhost:5000/");

      uint160_t addr2("0x22222");
      node.add_to_allow_list(addr2);
      node.set_stake_amount(addr2,  1'000'000'000'000'000'000ULL);
      node.set_url(addr2, "https://127.0.0.1:5001/");

      uint160_t addr3("0x33333");
      node.add_to_allow_list(addr3);
      node.set_stake_amount(addr3,  1'000'000'000'000'000'000ULL);
      node.set_url(addr3, "https://spoon.was.here.invalid:5002/");

      uint160_t addr4("0x44444");
      node.add_to_allow_list(addr4);
      node.set_stake_amount(addr4,  1'000'000'000'000'000'000ULL);
      node.set_url(addr4, "boop");

      uint160_t addr5("0x55555");
      node.add_to_allow_list(addr5);
      node.set_stake_amount(addr5,  1'000'000'000'000'000'000ULL);
      //empty URL

      co_await n.start_or_restart();

      co_await launch_cannon(target_num, 8000);
      //only 0x111111 and 0x22222 have valid URL; others should have been ignored
      BOOST_REQUIRE(upstream1.get_count() >= target_num/2-1 && upstream1.get_count() <= target_num/2+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/2-1 && upstream2.get_count() <= target_num/2+1);

      //add a staker with a duplicate url. that's no problem
      uint160_t addr6("0x66666");
      node.add_to_allow_list(addr6);
      node.set_stake_amount(addr6,  1'000'000'000'000'000'000ULL);
      node.set_url(addr6, "https://localhost:5000/");

      co_await n.wait();

      upstream1.reset_count();
      upstream2.reset_count();
      co_await launch_cannon(target_num, 8000);
      //only 0x111111 and 0x22222 have valid URL; others should have been ignored
      BOOST_REQUIRE(upstream1.get_count() >= target_num/3*2-1 && upstream1.get_count() <= target_num/3*2+1);
      BOOST_REQUIRE(upstream2.get_count() >= target_num/3-1 && upstream2.get_count() <= target_num/3+1);

   }, [](std::exception_ptr e) {
      if(e)
         std::rethrow_exception(e);
   });

   ctx.run();
}