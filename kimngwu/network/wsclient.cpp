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
  if (!PrivateKey_) {
    throw std::runtime_error("Private key is not loaded");
  }

  const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

  const std::string payload =
      "apiKey=" + ApiKey_ + "&timestamp=" + std::to_string(nowMs);

  const std::string signature = signED25519(PrivateKey_.get(), payload);

  // Build and send Binance session.logon JSON here.
}

boost::asio::awaitable<void> WsClient::readLoop() {}

void WsClient::reset() {
  // boost::beast::error_code ec;
  boost::beast::get_lowest_layer(Ws_).close();
  // Buffer_.clear();
  // Reconstruct fresh stream using stored executor and SSLContext_
  // Ws_ = WebSocketStream(Ws_.get_executor(), SSLContext_.get());
}

boost::asio::awaitable<boost::json::value>
WsClient::sendRequest(const boost::json::object& request)
{
    const std::string message = boost::json::serialize(request);

    ws_.text(true);

    co_await ws_.async_write(
        boost::asio::buffer(message),
        boost::asio::use_awaitable
    );

    boost::beast::flat_buffer buffer;

    co_await ws_.async_read(
        buffer,
        boost::asio::use_awaitable
    );

    const std::string response =
        boost::beast::buffers_to_string(buffer.data());

    boost::system::error_code ec;

    auto result = boost::json::parse(response, ec);

    if (ec) {
        throw std::runtime_error(
            "Invalid Binance JSON response: " + ec.message()
        );
    }

    co_return result;
}


std::string WsClient::signED25519(EVP_PKEY *privateKey,
                                  const std::string &payload) {
  EVP_MD_CTX *rawCtx = EVP_MD_CTX_new();

  if (!rawCtx) {
    throw std::runtime_error("Cannot create EVP_MD_CTX");
  }

  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(rawCtx,
                                                              EVP_MD_CTX_free);

  if (EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, privateKey) <=
      0) {
    throw std::runtime_error("Ed25519 signing initialization failed");
  }

  size_t signatureLength = 0;

  if (EVP_DigestSign(ctx.get(), nullptr, &signatureLength,
                     reinterpret_cast<const unsigned char *>(payload.data()),
                     payload.size()) <= 0) {
    throw std::runtime_error("Cannot determine signature length");
  }

  std::vector<unsigned char> signature(signatureLength);

  if (EVP_DigestSign(ctx.get(), signature.data(), &signatureLength,
                     reinterpret_cast<const unsigned char *>(payload.data()),
                     payload.size()) <= 0) {
    throw std::runtime_error("Ed25519 signing failed");
  }

  static constexpr char hex[] = "0123456789abcdef";

  std::string result(4 * ((signatureLength + 2) / 3) + 1, '\0');

  int encodedLength =
      EVP_EncodeBlock(reinterpret_cast<unsigned char *>(result.data()),
                      signature.data(), static_cast<int>(signatureLength));

  if (encodedLength < 0) {
    throw std::runtime_error("Base64 encoding failed");
  }

  result.resize(encodedLength);

  return result;
}

WsClient::EVP_PKEY_ptr WsClient::loadPrivateKey(const std::string &filePath) {
  using BIO_ptr = std::unique_ptr<BIO, decltype(&BIO_free)>;

  BIO_ptr bio(BIO_new_file(filePath.c_str(), "r"), BIO_free);

  if (!bio) {
    throw std::runtime_error("Cannot open private key: " + filePath);
  }

  EVP_PKEY *rawKey =
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);

  if (!rawKey) {
    throw std::runtime_error("Cannot read private key");
  }

  EVP_PKEY_ptr key(rawKey, EVP_PKEY_free);

  if (EVP_PKEY_base_id(key.get()) != EVP_PKEY_ED25519) {
    throw std::runtime_error("Expected an Ed25519 private key");
  }

  return key;
}



} // namespace kimngwu::network
