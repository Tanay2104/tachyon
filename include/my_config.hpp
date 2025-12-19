#pragma once

#include "containers/arena.hpp"
#include "containers/flat_buffer.hpp"
#include "containers/flat_hashmap.hpp"
#include "containers/intrusive_list.hpp"
#include "containers/lock_queue.hpp"
#include "containers/threadsafe_hashmap.hpp"
#include "network/tcpserver.hpp"

template <typename T> class testbuffer {
private:
  std::vector<T> buffer;

public:
  using value_type = T;
  testbuffer(size_t n = 1024) { buffer.reserve(n); }
  size_t size() { return buffer.size(); }
  T *begin() { return buffer.data(); }
  void insert(T *start, size_t count) {
    buffer.insert(buffer.end(), start, start + count);
  }
  void erase(size_t count) {
    buffer.erase(buffer.begin(), buffer.begin() + count);
  }
  void clear() { buffer.clear(); }
};
// Both test buffer and flat_buffer give the same kinds of wierd
// and wrong numbers for order id. price, 268800.

// Our sample config.
struct my_config {
  using MyPriceLevel = intrusive_list<ClientRequest>;
  using PriceLevelHierarchyType = std::vector<MyPriceLevel>;
  using EventQueue = threadsafe::stl_queue<ClientRequest>;
  using TradesQueue = threadsafe::stl_queue<Trade>;
  using ExecReportQueue = threadsafe::stl_queue<ExecutionReport>;
  using ArenaIdxMap = flat_hashmap<OrderId, uint32_t>;
  using ListIdxMap =
      flat_hashmap<OrderId, std::tuple<Side, Price, MyPriceLevel::iterator>>;

  using ArenaType = ArenaClass;
  using RxBufferType = flat_buffer<uint8_t>;
  using TxBufferType = flat_buffer<uint8_t>;
  using ClientMap =
      threadsafe::hashmap<ClientId,
                          ClientConnection<RxBufferType, TxBufferType> *>;
};
