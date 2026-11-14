#include "gateway/wsclient.hpp"
#include <boost/asio.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <print>

int main() {

  try {
	boost::asio::io_context& ioc;
	boost::asio::ssl::context &sslCtx;

	auto wsClient = std::make_shared<stock::gateway::websocket::WsClient>(ioc,sslCtx);

	wsClient->connect("example.com", "80");

  } catch (std::exception &e) {
    return 1;
  }
  return 0;
}
