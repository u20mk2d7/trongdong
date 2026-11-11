#include "gateway/wsclient.hpp"
#include <boost/asio.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <print>

int main() {

  try {
    boost::asio::io_context ioc;
    const auto client = std::make_shared<stock::gateway::websocket::WsClient>(
        ioc, "echo.websocket.events", "80");

    client->run();

    ioc.run();


  } catch (std::exception &e) {
    return 1;
  }
  return 0;
}
