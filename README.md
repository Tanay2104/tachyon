# Tachyon

Tachyon is a low-latency Limit Order Book (LOB) and Matching Engine written in C++20.

This project serves as an implementation study into high-frequency trading architectures, specifically focusing on **Data-Oriented Design**, **Cache Locality**, and **Lock-Free Concurrency**. It abandons standard STL containers (`std::map`, `std::list`) in the hot path in favor of custom, pre-allocated data structures to minimize memory latency and dynamic allocation overhead.

## Performance Metrics

Measurements taken on consumer hardware (Intel Core Ultra 7 155H, Linux 6.x).

* **Core Matching Latency:** ~3.2 ns (Micro-benchmarked via Google Benchmark, warm cache).
* **System Throughput:** ~665,000 requests/sec (End-to-End over TCP Loopback).
* **Capacity:** Sustained >3,000,000 active orders in memory without allocation-induced latency spikes.

## Architecture & Design Decisions

The engine is built around a single-threaded matching core fed by a non-blocking network reactor.

### 1. Memory Management (The Arena)

Standard `new`/`malloc` introduces fragmentation and non-deterministic latency. Tachyon uses a monolithic **Memory Arena** (`ArenaClass`).

* **Implementation:** A pre-allocated `std::vector` acts as the heap.
* **Addressing:** Orders are referenced via `uint32_t` indices ("handles") rather than 64-bit pointers. This reduces memory footprint and allows for linear memory traversals.
* **Recycling:** A specialized free-list (stack) handles O(1) allocation and deallocation of slots.

### 2. Custom Data Structures

Standard STL containers were replaced to eliminate pointer chasing and cache misses.

* **Flat Hash Map (`flat_hashmap`):**
  * An open-addressing, linear-probing hash map used for Order ID lookups.
  * Implements **Tombstone deletion** to allow slot reuse without rehashing.
  * Benchmarked at ~3x faster insertion and ~9x faster lookup than `std::unordered_map` for small keys.

* **Intrusive Linked List (`intrusive_list`):**
  * Used for price levels in the Order Book.
  * The `next` and `prev` pointers are embedded directly within the `ClientRequest` struct inside the Arena.
  * Eliminates the need for a separate `std::list` node allocation, ensuring better L1 cache density.

* **Lock-Free SPSC Queue:**
  * Used for inter-thread communication (Network Thread -> Engine Thread).
  * Implements a ring buffer using `std::atomic` with `memory_order_acquire` and `memory_order_release` semantics.
  * Decouples the TCP ingestion rate from the matching engine processing rate.

### 3. Networking (Epoll Reactor)

* **Mechanism:** Single-threaded Event Loop using raw Linux `epoll`.
* **Protocol:** Custom binary protocol with Big-Endian (Network Byte Order) serialization.
* **Buffering:** User-space Ring Buffers (`ClientConnection`) handle TCP stream fragmentation and coalescing (partial reads).
* **I/O:** Non-blocking sockets (`O_NONBLOCK`).

### 4. Logging

* Logging is moved off the critical path. The Engine pushes `Trade` and `ExecutionReport` structs to a lock-free queue.
* A dedicated Logging Thread drains this queue and handles file I/O.

## Supported Order Types

* **Limit Orders** (GTC - Good Till Cancelled)
* **Market Orders** (IOC - Immediate or Cancel)
* **Order Cancellation**

## Build & Run

### Dependencies

* C++20 Compiler (GCC 13+ / Clang 15+)
* CMake 3.20+
* Linux (Required for `epoll`, `endian.h`)

### Compilation

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Running the Server

```bash
./build/bin/tachyon
```

### Running the Client

```bash
./build/bin/trader_bot
```
### Logging
To events processed, log trades, execution reports as formatted text, make a logs directory. The appropriate text files will be stored here.
```bash
mkdir logs
```
### Benchmarks

Micro-benchmarks for individual components (OrderBook, Hashmap, Queues) are built using Google Benchmark.

```bash
./build/bin/benchmarkorderbook
./build/bin/benchmarkflathashmap
```

## Limitations / Future Work

This is a V1 implementation focused on the core matching loop. Current limitations include:

* **Backtesting**: No backtesting method is employed. Hence the "end to end" latency cannot be measured reliably. This is planned to be added in a future commit.
* **Strategy:** Currently the clients generate random orders drawn from a Gaussian. It would be much better to have clients use some basic strategy to generate orders.
* **Networking:** No market data is being broadcasted. Hence the clients are "blind" and don't have any strategy. A production system would use UDP Multicast for market data feeds.
* **Kernel Bypass:** Uses standard Linux sockets. Does not currently support DPDK or Solarflare OpenOnload.
* **Order Types:** FOK (Fill or Kill) and Stop orders are not yet implemented.
* **Error Handling:** Current implementation relies on basic exception handling; needs migration to `std::expected` or error codes for determinism.
