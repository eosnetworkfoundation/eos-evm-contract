#pragma once

#include <boost/process.hpp>
#include <boost/asio.hpp>

#include <filesystem>

class nginx {
public:
   nginx(const std::string& fallback_endpoint, const std::string& web3);
   boost::asio::awaitable<void> start_or_restart();
   boost::asio::awaitable<void> wait();
   ~nginx();

private:
   std::string fallback_endpoint;
   std::string web3;
   boost::process::child child;
   std::filesystem::path tmpdir;
   std::filesystem::path configfile;
};