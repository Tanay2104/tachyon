#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "containers/lock_queue.hpp"
#include "engine/constants.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"
// ============================================================================
// 1. Data Generation Helpers
// ============================================================================

static std::vector<int> GetRandomPrices(int count, int min, int max) {
  std::vector<int> prices;
  prices.reserve(count);
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(min, max);
  for (int i = 0; i < count; ++i) prices.push_back(dist(rng));
  return prices;
}

// Helper to construct a request easily
ClientRequest makeReq(OrderId oid, Side side, Price price, Quantity qty,
                      RequestType type = RequestType::New) {
  ClientRequest req;
  req.type = type;
  req.client_id = 1;
  req.time_stamp = 1000 + oid;

  if (type == RequestType::New) {
    req.new_order.order_id = oid;
    req.new_order.side = side;
    req.new_order.price = CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN + price;
    req.new_order.quantity = qty;
    req.new_order.order_type = OrderType::LIMIT;
    req.new_order.tif = TimeInForce::GTC;
  } else {
    req.order_id_to_cancel = oid;
  }
  return req;
}

// ============================================================================
// 2. OrderBook Micro-Benchmarks
// ============================================================================

// ----------------------------------------------------------------------------
// BENCHMARK: Adding Orders (Insertion)
// ----------------------------------------------------------------------------
static void BM_OrderBook_Add(benchmark::State& state) {
  const int N = state.range(0);
  auto prices = GetRandomPrices(N, 100, 200);

  // Pre-generate the objects to avoid measuring object construction time
  std::vector<ClientRequest> orders;
  orders.reserve(N);
  for (int i = 0; i < N; ++i) {
    // Alternating Buy/Sell at different prices so they don't match instantly
    // (We want to measure insertion into the Book, not matching)
    Side s = (i % 2 == 0) ? Side::BID : Side::ASK;
    Price p = (s == Side::BID) ? 90 : 210;
    orders.push_back(makeReq(i, s, p, 100));
  }

  for (auto _ : state) {
    state.PauseTiming();
    {
      OrderBook book;  // Fresh book every iteration

      state.ResumeTiming();  // START TIMER

      for (int i = 0; i < N; ++i) {
        book.add(orders[i]);
      }

      state.PauseTiming();  // STOP TIMER
    }  // Destructor runs here (unmeasured)
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_OrderBook_Add)->Range(1024, 8 << 11);

// ----------------------------------------------------------------------------
// BENCHMARK: Canceling Orders
// ----------------------------------------------------------------------------
static void BM_OrderBook_Cancel(benchmark::State& state) {
  const int N = state.range(0);
  auto prices = GetRandomPrices(N, 100, 150);

  std::vector<ClientRequest> orders;
  std::vector<ClientRequest> cancels;
  orders.reserve(N);
  cancels.reserve(N);

  for (int i = 0; i < N; ++i) {
    orders.push_back(makeReq(i, Side::ASK, prices[i], 100));
    cancels.push_back(makeReq(i, Side::ASK, 0, 0, RequestType::Cancel));
  }

  // Shuffle cancels to simulate random access
  std::mt19937 rng(12345);
  std::shuffle(cancels.begin(), cancels.end(), rng);

  for (auto _ : state) {
    state.PauseTiming();
    {
      OrderBook book;
      for (auto& o : orders) book.add(o);  // Fill book (unmeasured)
      ClientRequest temp_store;

      state.ResumeTiming();  // START TIMER

      for (auto& c : cancels) {
        // This measures the find_if linear scan + erase cost
        book.cancelOrder(c.order_id_to_cancel, temp_store);
      }

      state.PauseTiming();  // STOP TIMER
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_OrderBook_Cancel)->Range(1024, 8 << 11);

// ----------------------------------------------------------------------------
// BENCHMARK: Matching (Sweep)
// ----------------------------------------------------------------------------
static void BM_OrderBook_Match_Sweep(benchmark::State& state) {
  const int RESTING_COUNT = 1000;
  std::vector<ClientRequest> resting_orders;
  resting_orders.reserve(RESTING_COUNT);

  // 1000 small sell orders at 100
  for (int i = 0; i < RESTING_COUNT; ++i) {
    resting_orders.push_back(makeReq(i, Side::ASK, 100, 10));
  }

  // One big buy order to eat them all
  ClientRequest aggressor = makeReq(99999, Side::BID, 100, 10000);
  std::vector<std::pair<Trade, ClientRequest>> trades;
  trades.reserve(RESTING_COUNT);

  for (auto _ : state) {
    state.PauseTiming();
    {
      OrderBook book;
      for (auto& o : resting_orders) book.add(o);
      trades.clear();

      state.ResumeTiming();  // START TIMER

      // Measures the matching engine loop and tree deletion
      book.match(aggressor, trades);

      state.PauseTiming();  // STOP TIMER
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * RESTING_COUNT);
}
BENCHMARK(BM_OrderBook_Match_Sweep);

BENCHMARK_MAIN();
