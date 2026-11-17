#include "wsclient.hpp"

#include <chrono>
#include <exception>
#include <openssl/ssl.h>
#include <print>

namespace kimngwu::gateway {

WsClient::WsClient(boost::asio::io_context &ioc,
                   boost::asio::ssl::context &sslContext, std::string host,
                   std::string port, MessageCallback onMessage)
    : Host_(std::move(host)), Port_(std::move(port)), SSLContext_(sslContext),
      Ws_(ioc, sslContext), OnMessage_(std::move(onMessage)) //
{}

boost::asio::awaitable<void> WsClient::run() {
  boost::asio::steady_timer retryTimer(Ws_.get_executor());

  while (true) {
    try {

      co_await connectTCP();
      co_await handshakeTLS();
      co_await handshakeWS();
      co_await readLoop(); // Runs until disconnected or error

    } catch (const std::exception &e) {
      std::println(stderr, "{} | {} | {}", Host_, Port_, e.what());
      resetStream();
    }

    retryTimer.expires_after(std::chrono::seconds(1));
    co_await retryTimer.async_wait(boost::asio::use_awaitable);
  }
}

boost::asio::awaitable<void> WsClient::connectTCP() {
  // Get the lowest TCP layer of the WebSocket stream.
  // This gives us direct access to the underlying TCP socket.
  boost::beast::tcp_stream &lowestLayer = boost::beast::get_lowest_layer(Ws_);

  // Disable Nagle's algorithm.
  // This allows small TCP packets to be sent immediately
  // instead of waiting for more data to be accumulated.
  lowestLayer.socket().set_option(boost::asio::ip::tcp::no_delay(true));

  // Create a TCP resolver using the executor associated with Ws_.
  // The resolver converts a hostname and port into TCP endpoints.
  boost::asio::ip::tcp::resolver resolver(Ws_.get_executor());

  // Asynchronously resolve the hostname into one or more TCP endpoints.
  // co_await suspends this coroutine until DNS resolution completes,
  // without blocking the thread.
  const boost::asio::ip::tcp::resolver::results_type results =
      co_await resolver.async_resolve(Host_, Port_, boost::asio::use_awaitable);

  // Asynchronously connect the TCP socket to one of the resolved endpoints.
  // async_connect tries the available endpoints until a connection succeeds
  // or all connection attempts fail.
  // co_await suspends this coroutine until the connection completes.
  co_await lowestLayer.async_connect(results, boost::asio::use_awaitable);

  // TCP connection is now established.
  // The next step is usually the WebSocket handshake.
}

boost::asio::awaitable<void> WsClient::handshakeTLS() {}
boost::asio::awaitable<void> WsClient::handshakeWS() {}
boost::asio::awaitable<void> WsClient::readLoop() {}

void WsClient::resetStream() {
  // boost::beast::error_code ec;
  boost::beast::get_lowest_layer(Ws_).close();
  // Buffer_.clear();
  // Reconstruct fresh stream using stored executor and SSLContext_
  // Ws_ = WebSocketStream(Ws_.get_executor(), SSLContext_.get());
}

} // namespace kimngwu::gateway