#pragma once

#include "containers/intrusive_list.hpp"
#include "containers/lock_queue.hpp"
#include "engine/types.hpp"
#include <concepts>
#include <ranges>

template <typename L>
concept PriceLevel =
    requires(L level, std::ranges::iterator_t<L> itr, ClientRequest &clr) {
      { level.push_back(clr) };
      { level.remove(itr) } -> std::same_as<std::ranges::iterator_t<L>>;
      { level.size() } -> std::convertible_to<std::size_t>;
    };

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

template <typename C>
concept TachyonConfig = requires {
  // Checking if it has a price level hierarchy.
  typename C::PriceLevelHierarchyType;
  requires PriceLevelHierarchy<typename C::PriceLevelHierarchyType>;

  // check if it has the various queues we need.
  typename C::EventQueue;
  requires ThreadSafeQueue<typename C::EventQueue, ClientRequest>;

  typename C::TradesQueue;
  requires ThreadSafeQueue<typename C::TradesQueue, Trade>;

  typename C::ExecReportQueue;
  requires ThreadSafeQueue<typename C::ExecReportQueue, ExecutionReport>;
};

// Our sample config.
struct my_config {
  using PriceLevelHierarchyType = std::vector<intrusive_list<ClientRequest>>;
  using EventQueue = threadsafe::stl_queue<ClientRequest>;
  using TradesQueue = threadsafe::stl_queue<Trade>;
  using ExecReportQueue = threadsafe::stl_queue<ExecutionReport>;
};

// Now we initialise our policies.

template <typename L>
concept Logger = requires(L logger, ClientRequest &incoming,
                          ClientRequest &resting, Quantity trade_quantity) {
  { logger.logNotFound(incoming) };
  { logger.logSelfTrade(incoming) };
  { logger.logSelfTrade(incoming) }; // User tried self trade
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
