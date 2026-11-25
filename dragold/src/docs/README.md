dnse_client/
├── src/
    ├── CMakeLists.txt
    ├── main.cpp
    ├── gateway/       # Boost.Asio I/O thread (Network RX/TX)
    ├── protocol/      # simdjson raw payload decoding
    ├── strategy/      # <--- HERE: Quantitative Decision & Math Engine
    │   ├── QuantEngine.hpp
    │   └── SignalEvaluator.hpp
    └── core/          # Lock-free SPSC queues, fixed-point math, thread pinning


How Data Flows Through the Hot Path:
Network RX (IO Thread): WsClient receives raw WebSocket binary/text frames from DNSE.

Parsing (Zero-Copy): MessageParser decodes the packet using simdjson.

The Hot Path (Strategy Thread): The parsed market data (e.g., best bid/ask, trade ticks) is pushed via a lock-free Single-Producer Single-Consumer (SPSC) ring buffer straight into QuantEngine.

Decision: QuantEngine evaluates the mathematical model (e.g., spread arbitrage, moving averages, order book imbalance) using fixed-point arithmetic.

Execution: If a threshold is met, an order packet is built and fired back out through the gateway.


[Gateway / Connector]
   ├── owns: net::io_context ioc{1}
   ├── owns: ssl::context sslCtx
   ├── owns: SPSCQueue ringBuffer
   └── spawns: WsClient("stream.binance.com", "9443", "/ws/btcusdt@trade", onMessageCallback)
                  │
                  ▼ (inside WsClient::run())
            1. DNS Resolve
            2. TCP Connect (+ TCP_NODELAY)
            3. TLS Handshake (with SNI)
            4. WebSocket Upgrade (no compression)
            5. Read Loop ──► calls onMessage(std::span<const std::byte>)
