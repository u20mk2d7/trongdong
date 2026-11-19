#include "wsclient.hpp"

#include <chrono>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/json.hpp>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>

#include <simdjson.h>

namespace kimngwu::gateway {

WsClient::WsClient(boost::asio::io_context &ioc,
                   boost::asio::ssl::context &sslContext, std::string host,
                   std::string port, MessageCallback onMessage)
    : Host_(std::move(host)), Port_(std::move(port)), SSLContext_(sslContext),
      Ws_(ioc, sslContext), OnMessage_(std::move(onMessage)) //
{
  SSLContext_.get().set_default_verify_paths();

  // Set the verification mode to verify the peer's certificate
  SSLContext_.get().set_verify_mode(boost::asio::ssl::verify_peer);
}

boost::asio::awaitable<void> WsClient::run() {
  co_await connectTCP();
  co_await handshakeTLS();
  co_await handshakeWS();
  co_await logon();    // Authenticate session once via Ed25519
  co_await readLoop(); // Runs until disconnected or error
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

boost::asio::awaitable<void> WsClient::handshakeTLS() {
  if (!SSL_set_tlsext_host_name(Ws_.next_layer().native_handle(),
                                Host_.c_str())) //
  {
    //
    boost::beast::error_code err{
        static_cast<int>(::ERR_get_error()),
        boost::asio::ssl::error::get_stream_category() //
    };

    throw boost::beast::system_error{err};
  }

  co_await Ws_.next_layer().async_handshake(
      boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);
}

boost::asio::awaitable<void> WsClient::handshakeWS() {

  Ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
      boost::beast::role_type::client));

  co_await Ws_.async_handshake(Host_, "/ws-api/v3", boost::asio::use_awaitable);
}

boost::asio::awaitable<void> WsClient::logon() {
  const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
  std::string payLoadToSign =
      "apiKey=" + ApiKey_ + "&timestamp=" + std::to_string(nowMs);
  std::string signature = signED25519(PrivateKey_, payLoadToSign);
}

boost::asio::awaitable<void> WsClient::readLoop() {}

void WsClient::reset() {
  // boost::beast::error_code ec;
  boost::beast::get_lowest_layer(Ws_).close();
  // Buffer_.clear();
  // Reconstruct fresh stream using stored executor and SSLContext_
  // Ws_ = WebSocketStream(Ws_.get_executor(), SSLContext_.get());
}

std::string WsClient::signED25519(EVP_PKEY *pkey, std::string &payload) {
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, pkey);

  size_t sigLen = 64; // Ed25519 signatures are always 64 bytes
  std::vector<unsigned char> sig(sigLen);

  EVP_DigestSign(mdctx, sig.data(), &sigLen,
                 reinterpret_cast<const unsigned char *>(payload.data()),
                 payload.size());
  EVP_MD_CTX_free(mdctx);

  // Base64 encode using Boost.Beast's built-in utility
  std::string encoded;
  encoded.resize(boost::beast::detail::base64::encoded_size(sigLen));
  boost::beast::detail::base64::encode(encoded.data(), sig.data(), sigLen);
  return encoded;
}

EVP_PKEY *load_private_key_from_pem(const std::string &pem_str) {
  BIO *bio = BIO_new_mem_buf(pem_str.data(), static_cast<int>(pem_str.size()));
  if (!bio) {
    return nullptr;
  }

  EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  return pkey; // Caller owns pkey; free with EVP_PKEY_free(pkey)
}
} // namespace kimngwu::gateway
