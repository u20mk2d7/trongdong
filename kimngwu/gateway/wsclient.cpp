#include "wsclient.hpp"
#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <string>

namespace stock::gateway::websocket {

boost::asio::awaitable<void> WsClient::connect(std::string host,
                                               std::string port) {

  boost::asio::ip::tcp::resolver resolver(Ioc_);

  auto endpoints =
      co_await resolver.async_resolve(host, port, boost::asio::use_awaitable);

  if (endpoints.empty()) {
    std::cerr << "Resolution failed: No endpoints found.\n";
    co_return;
  }
  std::cout << "Successfully resolved " << host
            << " to the following endpoints:\n";
  for (const auto &entry : endpoints) {
    auto ep = entry.endpoint();
    std::cout << " -> IP: " << ep.address().to_string()
              << " | Port: " << ep.port() << " ("
              << (ep.address().is_v6() ? "IPv6" : "IPv4") << ")\n";
  }
}

} // namespace stock::gateway::websocket
