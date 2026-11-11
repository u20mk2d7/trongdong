#include "wsclient.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <iostream>

namespace stock::gateway::websocket {

void WsClient::run() {
  std::cout << "[WsClient] Starting resolution for " << Host_ << ":" << Port_
            << "...\n";
  Resolver_.async_resolve(Host_, Port_,
                          boost::beast::bind_front_handler(&WsClient::onResolve,
                                                           shared_from_this()));
}

void WsClient::stop() {}

void WsClient::onResolve(
    boost::beast::error_code errorCode,
    const boost::asio::ip::tcp::resolver::results_type &results) {

  // DNS resolution and service lookup, human-readable hostname -> network
  // endpoint (TCP Connection)

  // 1. translate hostname to IP Addr (DNS Lookup)
  // Synchronous: Resolver_.resolve(Host_, Port_);
}

void WsClient::onConnect() {}
void WsClient::onSslHandshake() {}

} // namespace stock::gateway::websocket
