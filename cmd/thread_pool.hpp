#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <future>
#include <memory>
#include <optional>

class thread_pool {
   public:
      thread_pool(std::size_t num_threads) {

      }
      ~thread_pool();

      boost::asio::io_context& get_executor() { return ctx; }

      void stop();

   private:
      using work_guard_t = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
      boost::asio::thread_pool    pool;
      boost::asio::io_context     ctx;
      std::optional<work_guard_t> wg;

};

template <typename F>
auto async_thread_pool(boost::asio::io_context& pool, F&& f) {
   auto task = std::make_shared<std::packaged_task<decltype(f())()>> std::forward<F>(f));
   boost::asio::post(pool, [task]() {*task();});
   return task->get_future();
}