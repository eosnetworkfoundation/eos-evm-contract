#pragma once

#include <boost/asio.hpp>

boost::asio::awaitable<void> launch_cannon(unsigned count, uint16_t port);