#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>

/**
 * WsClient (Network Transport Layer): Purely handles the WebSocket
 * lifecycle—DNS resolve, TCP connect, TLS handshake, WebSocket upgrade, reading
 * frames in a loop, and passing raw message bytes up to the caller. It doesn't
 * need to know anything about Binance JSON or trading logic.
 */

namespace kimngwu::gateway {

class WsClient : public std::enable_shared_from_this<WsClient> {

public:
  // Layer 1 (Bottom): Raw TCP socket with built-in timeout support
  using TCPLayer = boost::beast::tcp_stream;

  // Layer 2 (Middle): TLS Encryption, Decryption, handshake, TLS wrapped TCP
  using TLSLayer = boost::asio::ssl::stream<TCPLayer>;

  // Layer 3 (Top): WebSocket framing wrapped TLS stream messages, ping/pong
  using WebSocketStream = boost::beast::websocket::stream<TLSLayer>;

  // Callback passes raw bytes directly from the read buffer (zero-copy view)
  using MessageCallback = std::function<void(std::span<const std::byte>)>;

public:
  WsClient(boost::asio::io_context &ioc, boost::asio::ssl::context &sslContext,
           std::string host, std::string port, MessageCallback onMessage);

  ~WsClient() = default;

  WsClient(const WsClient &) = delete; // copy constructor
  WsClient(WsClient &&) = delete;      // move constructor

  WsClient &operator=(const WsClient &) = delete; // copy assignment
  WsClient &operator=(WsClient &&) = delete;      // move assignment

  // Starts the full connection + read loop coroutine
  boost::asio::awaitable<void> run();
  void resetStream();

private:
  std::string Host_;
  std::string Port_;
  std::string Target_;

  std::reference_wrapper<boost::asio::ssl::context> SSLContext_;

  WebSocketStream Ws_;

  MessageCallback OnMessage_;

  boost::beast::flat_buffer Buffer_;

  // single-responsibility private coroutines
  boost::asio::awaitable<void> connectTCP();
  boost::asio::awaitable<void> handshakeTLS();
  boost::asio::awaitable<void> handshakeWS();
  boost::asio::awaitable<void> readLoop();
};
} // namespace kimngwu::gateway
