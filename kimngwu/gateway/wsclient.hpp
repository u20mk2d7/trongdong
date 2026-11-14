#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>

namespace stock::gateway::websocket {

class WsClient : public std::enable_shared_from_this<WsClient> {

private:
  boost::asio::io_context &Ioc_;
  boost::asio::ssl::context &SslCtx_;
  boost::asio::ip::tcp::socket Socket_;

  std::string Host_;
  std::string Port_;

public:
  WsClient(boost::asio::io_context &ioc, boost::asio::ssl::context &sslCtx)
      : Ioc_(ioc), SslCtx_(sslCtx), Socket_(ioc) {}

  boost::asio::awaitable<void> connect(std::string host, std::string port);
};
} // namespace stock::gateway::websocket
