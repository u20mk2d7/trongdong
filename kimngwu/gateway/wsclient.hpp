#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>

namespace stock::gateway::websocket {

class WsClient : public std::enable_shared_from_this<WsClient> {
private:
  boost::asio::io_context &Ioc_;
  boost::asio::ip::tcp::resolver Resolver_;
  boost::beast::websocket::stream<boost::asio::ip::tcp::socket> Ws_;

  std::string Host_;
  std::string Port_;

  void onResolve(boost::beast::error_code errorCode,
                 const boost::asio::ip::tcp::resolver::results_type &results);
  void onConnect();
  void onSslHandshake();

public:
  WsClient(boost::asio::io_context &ioc, std::string host, std::string port)
      : Ioc_(ioc), Resolver_(boost::asio::make_strand(ioc)),
        Ws_(boost::asio::make_strand(ioc)), Host_(std::move(host)),
        Port_(std::move(port))

  {}

  void run();
  void stop();
};
} // namespace stock::gateway::websocket
