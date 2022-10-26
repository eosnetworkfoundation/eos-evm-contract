#include "nginx.hpp"

#include <fstream>

nginx::nginx(const std::string& fallback_endpoint, const std::string& web3) : fallback_endpoint(fallback_endpoint), web3(web3) {
   tmpdir = std::filesystem::temp_directory_path() / "evmtest";
   std::filesystem::create_directories(tmpdir);

   configfile = tmpdir / "config.conf";

   {
      std::ofstream ofs(configfile);
      ofs << "pid " << tmpdir/"pid" << ";" << std::endl;
      ofs << "error_log " << tmpdir/"elog" << " debug;" << std::endl;
      ofs << "events {}" << std::endl;
      ofs << "http {" << std::endl;
      ofs << "  upstream backend {\n" << std::endl;
      ofs << "$STAKERS" << std::endl;
      ofs << "  }" << std::endl;
      ofs << "  server {" << std::endl;
      ofs << "    location / {" << std::endl;
      ofs << "      proxy_pass http://backend;" << std::endl;
      ofs << "    }" << std::endl;
      ofs << "    access_log " << tmpdir/"nlog" << ";" << std::endl;
      ofs << "  }" << std::endl;
      ofs << "  client_body_temp_path " << tmpdir/"cb" << ";" << std::endl;
      ofs << "  fastcgi_temp_path " << tmpdir/"fg" << ";" << std::endl;
      ofs << "  uwsgi_temp_path " << tmpdir/"uw" << ";" << std::endl;
      ofs << "  scgi_temp_path " << tmpdir/"sc" << ";" << std::endl;
      ofs << "}" << std::endl;
   };
}

boost::asio::awaitable<void> nginx::start_or_restart() {
   if(child.valid())
      kill(child.native_handle(), SIGHUP);
   else
      child = boost::process::child("/usr/bin/node", "main.mjs", "-a", "0x0000000000000000000000000000000000000000", "-c", configfile.string(),
                                    "-n", "/usr/bin/nginx", "-f", fallback_endpoint, "-r", web3);

   //give it a moment to settle
   boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
   timer.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(2));
   co_await timer.async_wait(boost::asio::use_awaitable);
}

//the idea is that this waits long enough for the config to have been naturally updated. with the current update
// rate of 1s, let's wait around 2s
boost::asio::awaitable<void> nginx::wait() {
   boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
   timer.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(2));
   co_await timer.async_wait(boost::asio::use_awaitable);
}

nginx::~nginx() {
   std::filesystem::remove_all(tmpdir);
   if(child.valid()) {
      kill(child.native_handle(), SIGTERM);
      child.wait();
   }
}
