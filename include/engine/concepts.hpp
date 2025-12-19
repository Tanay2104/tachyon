#pragma once

#include "engine/types.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>

template <typename L>
concept PriceLevel =
    requires(L level, std::ranges::iterator_t<L> itr, ClientRequest &clr) {
      { level.push_back(clr) };
      { level.erase(itr) } -> std::same_as<std::ranges::iterator_t<L>>;
      { level.size() } -> std::convertible_to<std::size_t>;
    };
// TODO: maybe change remove -> erase for consistency.

template <typename H>
concept PriceLevelHierarchy = requires(H hierarchy, Price price) {
  typename H::value_type;
  { hierarchy[price] } -> std::same_as<typename H::value_type &>;
  requires PriceLevel<typename H::value_type>;
};

template <typename Q, typename T>
concept ThreadSafeQueue = requires(Q &queue, T &item) {
  { queue.push(item) };
  { queue.try_pop(item) } -> std::convertible_to<bool>;
};

template <typename M, typename K, typename V>
concept Map = requires(M &map, K &key, V &value) {
  { map.insert({key, value}) };
  { map.contains(key) } -> std::convertible_to<bool>;
  { map.at(key) } -> std::same_as<V &>;
  { map.erase(key) };
};

template <typename A>
concept Arena = requires(A arena, const ClientRequest &incoming, uint32_t idx) {
  { arena.allocateSlot(incoming) } -> std::convertible_to<uint32_t>;
  { arena.freeSlot(idx) };
};

template <typename B>
concept RxTxBuffer =
    requires(B rxtxbuffer, typename B::value_type *buff_start, size_t count) {
      { rxtxbuffer.insert(buff_start, count) };
      { rxtxbuffer.size() } -> std::convertible_to<size_t>;
      { rxtxbuffer.erase(count) };
      { rxtxbuffer.begin() } -> std::same_as<typename B::value_type *>;
    };

template <typename C>
concept TachyonConfig = requires {
  // Checking if it has a price level hierarchy.
  typename C::PriceLevelHierarchyType;
  typename C::PriceLevelHierarchyType::value_type;
  requires PriceLevelHierarchy<typename C::PriceLevelHierarchyType>;

  // NOTE: these queues must be threadsafe.
  typename C::EventQueue;
  requires ThreadSafeQueue<typename C::EventQueue, ClientRequest>;

  typename C::TradesQueue;
  requires ThreadSafeQueue<typename C::TradesQueue, Trade>;

  typename C::ExecReportQueue;
  requires ThreadSafeQueue<typename C::ExecReportQueue, ExecutionReport>;

  typename C::ArenaIdxMap;
  requires Map<typename C::ArenaIdxMap, OrderId, uint32_t>;

  typename C::ListIdxMap;
  requires Map<
      typename C::ListIdxMap, OrderId,
      std::tuple<Side, Price,
                 std::ranges::iterator_t<
                     typename C::PriceLevelHierarchyType::value_type>>>;

  typename C::ArenaType;
  requires Arena<typename C::ArenaType>;

  typename C::RxBufferType;
  requires RxTxBuffer<typename C::RxBufferType>;

  typename C::TxBufferType;
  requires RxTxBuffer<typename C::TxBufferType>;

  // NOTE: this must be threadsafe.
  typename C::ClientMap;
  requires Map<typename C::ClientMap, ClientId,
               typename C::ClientMap::mapped_type>;
};

// Now we initialise our policies.

template <typename L>
concept Logger = requires(L logger, ClientRequest &incoming,
                          ClientRequest &resting, Quantity trade_quantity) {
  { logger.logNotFound(incoming) };
  { logger.logSelfTrade(incoming) };
  { logger.logInvalidOrder(incoming) };
  { logger.logNewOrder(incoming) };    // New order logged
  { logger.logCancelOrder(incoming) }; // Cancellation successful.
  { logger.logTrade(resting, incoming, trade_quantity) };
  { logger.writeProcessedEventsLogs() };
  { logger.writeProcessedEventsLogsContinuous() };
};

// To be used properly when I move to modules.
template <typename P>
concept TachyonPolicies = requires {
  typename P::LoggerPolicy;
  requires Logger<typename P::LoggerPolicy>;
};
struct my_policy {};
