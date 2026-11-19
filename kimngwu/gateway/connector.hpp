#pragma once

/**
 * Gateway / Connector (Caller): Owns the io_context, ssl::context, and the SPSC
 * ring buffer. It decides which host, port, and stream path to connect to
 * (e.g., stream.binance.com, 9443, /ws/btcusdt@trade), creates a WsClient, and
 * gives it a callback (or reference to push raw bytes) whenever a message
 * arrives.
 *
 */
namespace kimngwu::gateway {

class Connector {};

} // namespace kimngwu::gateway