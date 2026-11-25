#include "wsclient.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <simdjson.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace kimngwu::network {

SessionConnector::SessionConnector(boost::asio::io_context &ioc,
                                   boost::asio::ssl::context &sslContext,
                                   std::string host, std::string port,
                                   MessageCallback onMessage)
    : Host_(std::move(host)), Port_(std::move(port)), SSLContext_(sslContext),
      Ws_(ioc, sslContext), OnMessage_(std::move(onMessage)) //
{
  SSLContext_.get().set_default_verify_paths();
  // Set the verification mode to verify the peer's certificate
  SSLContext_.get().set_verify_mode(boost::asio::ssl::verify_peer);
}

boost::asio::awaitable<void> SessionConnector::start() {
  co_await connectTCP();
  co_await handshakeTLS();
  co_await handshakeWS();
  co_await logon();    // Authenticate session once via Ed25519
  co_await readLoop(); // Runs until disconnected or error
}

boost::asio::awaitable<void> SessionConnector::connectTCP() {
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

boost::asio::awaitable<void> SessionConnector::handshakeTLS() {
  // 1. Hostname verification callback
  auto &sslStream = Ws_.next_layer();

  // Verify the server cert matches Host_
  sslStream.set_verify_callback(
      boost::asio::ssl::host_name_verification(Host_));

  // 2. Server Name Indication (SNI).
  // Set SNI so the server selects the certificate for Host_
  if (!SSL_set_tlsext_host_name(Ws_.next_layer().native_handle(),
                                Host_.c_str())) //
  {
    //
    boost::beast::error_code err{
        static_cast<int>(::ERR_get_error()),
        boost::asio::ssl::error::get_stream_category() //
    };

    throw boost::system::system_error{err, "SSL_set_tlsext_host_name failed"};
  }

  // 3. Handshake timeout async_handshake lowest TCP layer
  co_await sslStream.async_handshake(boost::asio::ssl::stream_base::client,
                                     boost::asio::use_awaitable);

  boost::beast::get_lowest_layer(Ws_).expires_never();
}

boost::asio::awaitable<void> SessionConnector::handshakeWS() {
  // Optional: configure default decorator or read/write options on Ws_ before
  // handshake
  Ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
      boost::beast::role_type::client));

  // Binance WebSocket API endpoint path (e.g. "/ws-api/v3")
  co_await Ws_.async_handshake(Host_, "/ws-api/v3", boost::asio::use_awaitable);

  boost::beast::get_lowest_layer(Ws_).expires_never();
}

boost::asio::awaitable<void> SessionConnector::logon() {
  //
  const auto timestampMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  // build stored canonical query string (excluding "sign")
  // key in ascending order: "apiKey", "timestamp"
  const std::string payload =
      "apiKey=" + ApiKey_ + "&timestamp=" + std::to_string(timestampMs);

  // sign payload UTF-8 bytes with Ed25519 prv key -> Base64 str
  const std::string signatureBase64 = signEd25519Base64(payload);
}

boost::asio::awaitable<void> SessionConnector::readLoop() {}

void SessionConnector::close() {}

boost::asio::awaitable<boost::json::value>
SessionConnector::sendRequest(const boost::json::object &request) {
  const std::string message = boost::json::serialize(request);

  Ws_.text(true);

  co_await Ws_.async_write(boost::asio::buffer(message),
                           boost::asio::use_awaitable);

  boost::beast::flat_buffer buffer;

  co_await Ws_.async_read(buffer, boost::asio::use_awaitable);

  const std::string response = boost::beast::buffers_to_string(buffer.data());

  boost::system::error_code ec;

  auto result = boost::json::parse(response, ec);

  if (ec) {
    throw std::runtime_error("Invalid Binance JSON response: " + ec.message());
  }

  co_return result;
}

} // namespace kimngwu::network
