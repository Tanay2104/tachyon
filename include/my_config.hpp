#pragma once

#include "containers/arena.hpp"
#include "containers/flat_buffer.hpp"
#include "containers/flat_hashmap.hpp"
#include "containers/intrusive_list.hpp"
#include "containers/lock_queue.hpp"
#include "containers/threadsafe_hashmap.hpp"
#include "network/tcpserver.hpp"

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
