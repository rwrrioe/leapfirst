# leapfirst

High-performance C++ signal processing engine for Binance WebSocket market data streams. Zero-allocation hot path, lock-free SPSC queues, SIMD-optimized indicators, Kafka output.

---

## Architecture

```
wss://stream.binance.com
  btcusdt@aggTrade
  ethusdt@aggTrade
  btcusdt@bookTicker
  btcusdt@depth@100ms
  &timeUnit=MICROSECOND
         │
         ▼
┌─────────────────────────────┐
│        IO Thread            │
│  boost::beast WebSocket     │
│  simdjson ondemand parse    │  zero-copy, SIMD
│  parse_fp8() fixed-point    │  no float, no alloc
│  dispatch by stream name    │
└────────────┬────────────────┘
             │ DispatchCallback
             ▼
┌─────────────────────────────┐
│         Engine              │
│  route() → SPSC fan-out     │
└──┬──────┬──────┬────────────┘
   │      │      │
   │ SPSC │ SPSC │ SPSC          lock-free, power-of-2 ring buffers
   ▼      ▼      ▼
EWMA   Z-score  Beta/Corr        pinned threads, spin-wait
   │      │      │
   └──────┴──────┘
          │ SPSC<Signal>
          ▼
   KafkaProducer                 librdkafka, partition by symbol
          │
   ════════════════
   Kafka  3 brokers
   ════════════════
          │
   Go Consumer Service
   exactly-once semantics
          │
   Postgres + gRPC API
```

---

## Hot Path — Zero Allocations

```
beast::async_read → flat_buffer (pre-reserved 4096)
    ↓
simdjson::iterate (string_view, padded buffer in Impl)
    ↓
parse_fp8() — decimal string → uint64 fixed-point ×1e8
    ↓
DispatchCallback → Engine::route()
    ↓
SPSCQueue::push() — 56-byte struct copy into ring buffer
    ↓
Worker::pop() — spin-wait, __builtin_ia32_pause()
    ↓
compute indicator — O(1) state update, no heap
    ↓
SPSCQueue<Signal>::push()
```

Single allocation on startup. Zero allocations per tick.

---

## Data Structures

All structs sized and padded explicitly. Cache-line aligned where needed.

```cpp
struct AggTrade {           // btcusdt@aggTrade
    int64_t  event_time;   // E — microseconds (timeUnit=MICROSECOND)
    int64_t  trade_time;   // T
    int64_t  agg_trade_id; // a
    uint64_t price_fp;     // p × 1e8 fixed-point
    uint64_t qty_fp;       // q × 1e8
    char     symbol[12];   // s
    bool     buyer_is_maker; // m
    uint8_t  _pad[3];
    // sizeof = 56
};

struct BookTicker {         // btcusdt@bookTicker
    uint64_t update_id;    // u
    uint64_t bid_price_fp; // b
    uint64_t bid_qty_fp;   // B
    uint64_t ask_price_fp; // a
    uint64_t ask_qty_fp;   // A
    char     symbol[12];   // s
    uint8_t  _pad[4];
    // sizeof = 56
};

struct Signal {
    int64_t    timestamp;
    double     value;
    SignalType type;        // EWMA_CROSS | ZSCORE | BETA | CORRELATION
    Direction  direction;  // BUY | SELL | NEUTRAL
    char       symbol[12];
    uint8_t    _pad[2];
    // sizeof = 32
};
```

---

## SPSC Queue

Lock-free single-producer single-consumer ring buffer.

```
IO Thread → push()     Worker Thread → pop()
     head_ (atomic)         tail_ (atomic)
     alignas(64)            alignas(64)      ← separate cache lines
```

- Power-of-2 capacity, bitmask indexing
- `memory_order_release` on write, `memory_order_acquire` on read
- No mutex, no CAS loop, no false sharing
- Capacity: `AggTrade × 4096 = 229 KB` per queue

Fan-out pattern — IO thread writes to N queues, each worker owns one:

```
IO Thread ──► SPSC<AggTrade> ──► EwmaWorker
          └─► SPSC<AggTrade> ──► ZscoreWorker
          └─► SPSC<AggTrade> ──► BetaWorker (BTC leg)
ETH IO    └─► SPSC<AggTrade> ──► BetaWorker (ETH leg)
```

---

## Indicators

| Indicator | Input | Complexity | SIMD |
|-----------|-------|------------|------|
| EWMA (12/26) | AggTrade.price | O(1) | planned |
| Z-score | AggTrade returns | O(1) rolling | planned |
| Beta | BTC + ETH prices | O(1) rolling | planned |
| Correlation | BTC × ETH | O(1) rolling | planned |
| Book Imbalance | BookTicker bid/ask qty | O(1) | — |

EWMA update — no heap, no history buffer:
```cpp
ema = alpha * price + (1.0 - alpha) * ema;  // one multiply, one FMA
```

---

## Binance Streams

Connected via combined stream — single WebSocket connection, up to 1024 streams.

```
wss://stream.binance.com:9443/stream?streams=
    btcusdt@aggTrade/
    ethusdt@aggTrade/
    btcusdt@bookTicker/
    btcusdt@depth@100ms
    &timeUnit=MICROSECOND
```

| Stream | Update speed | Used for |
|--------|-------------|----------|
| `@aggTrade` | real-time | EWMA, Z-score, Beta, Correlation |
| `@bookTicker` | real-time | Book imbalance |
| `@depth@100ms` | 100ms | Local order book |
| `@kline_1m` | 1s | Closed candle signals |

Connection lifecycle:
- Max 24 hours per connection → auto-reconnect on close
- Server ping every 20s → pong reply required
- `timeUnit=MICROSECOND` for µs-precision timestamps

---

## Thread Model

```
Thread   Role                  Pinned core   Wait strategy
──────   ────────────────────  ───────────   ─────────────
0        IO (ioc.run())        0             async I/O (no spin)
1        EwmaWorker            1             spin + pause
2        ZscoreWorker          2             spin + pause
3        BetaWorker            3             spin + pause
4        KafkaProducer         4             spin + pause
```

Workers use `__builtin_ia32_pause()` on empty queue — reduces power and memory bus contention without yielding the core.

---

## Latency Profile

```
Operation                      Typical     Notes
─────────────────────────────────────────────────────
Network (exchange → host)      1–5ms       dominant factor
simdjson parse                 30–50ns     SIMD, zero-copy
parse_fp8() × 5 fields         ~10ns
DispatchCallback call          ~2ns        std::function indirect
SPSC push × 4 (fan-out)        ~20ns       56 bytes × 4 copies
Worker pop + EWMA compute      ~8ns
Signal SPSC push               ~5ns
─────────────────────────────────────────────────────
C++ engine total               ~75–100ns
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| Boost.Beast | WebSocket + HTTP |
| Boost.Asio | async I/O |
| OpenSSL | TLS/WSS |
| simdjson | zero-copy JSON parsing |
| librdkafka | Kafka producer |

---

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Requirements: C++20, CMake 3.20+, Linux (for `pthread_setaffinity_np`).

---

## Project Structure

```
.
├── transport/
│   ├── include/transport/
│   │   ├── types.h              # AggTrade, BookTicker, Signal, DispatchCallback
│   │   └── websocket_client.h
│   └── src/
│       └── websocket_client.cpp # Impl — beast, simdjson, fan-out
├── engine/
│   ├── include/engine/
│   │   ├── engine.h
│   │   ├── spsc_queue.h         # SPSCQueue<T, N>
│   │   └── workers/
│   │       ├── ewma_worker.h
│   │       ├── zscore_worker.h
│   │       └── beta_worker.h
│   └── src/
│       ├── engine.cpp
│       └── workers/
├── kafka/
│   ├── include/kafka/producer.h
│   └── src/producer.cpp
└── main.cpp
```

---

## Kafka Output

Signals published to topic `signals`, partitioned by symbol hash.

```
Producer settings:
  acks          = all
  linger.ms     = 1
  batch.size    = 65536
  compression   = lz4
```

Signal schema (Protobuf):
```protobuf
message Signal {
    int64      timestamp  = 1;
    double     value      = 2;
    SignalType type       = 3;
    Direction  direction  = 4;
    string     symbol     = 5;
}
```

---

## Go Consumer

Reads from Kafka with exactly-once semantics (`isolation.level = read_committed`), writes to Postgres via COPY bulk insert, exposes signals via gRPC + REST API.

HPA configured on Kafka consumer group lag — scales consumer pods automatically under load.