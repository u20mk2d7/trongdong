#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <memory>
#include <string>

namespace stock::gateway::websocket {

class WsClient : public std::enable_shared_from_this<WsClient> {

private:
  boost::asio::io_context &Ioc_;
  boost::asio::ssl::context &SslContext_;

  boost::beast::websocket::stream<
      boost::asio::ssl::stream<boost::beast::tcp_stream>>
      Ws_;

  std::string Host_;
  std::string Port_;

public:
  WsClient(boost::asio::io_context &ioc, boost::asio::ssl::context &sslContext,
           std::string host, std::string port)
      : Ioc_(ioc), SslContext_(sslContext),
        Ws_(boost::asio::strand(ioc), SslContext_), Host_(std::move(host)),
        Port_(std::move(port)) {}
};
} // namespace stock::gateway::websocket
