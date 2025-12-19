#include "engine/engine.hpp"
#include "my_config.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <thread>

#include "containers/lock_queue.hpp"
#include "engine/concepts.hpp"
#include "engine/constants.hpp"
#include "engine/types.hpp"

extern std::atomic<bool> start_exchange;
extern std::atomic<bool> keep_running;
uint64_t batch_size = 100000;

template <TachyonConfig config>
Engine<config>::Engine(config::EventQueue &ev_q,
                       config::EventQueue &prcs_events,
                       OrderBook<config> &orderbook, LoggerClass<config> &lgr)
    : event_queue(ev_q), logger(lgr), processed_events(prcs_events),
      orderbook(orderbook) {
  trades_buffer.reserve(MAX_TRADE_BUFFER_SIZE);
}

template <TachyonConfig config> Engine<config>::~Engine() = default;

// WARNING: Check for seg faults becase we ignore the boolean returned by insert
// in map and only use it.

template <TachyonConfig config>
void Engine<config>::handle_GTC_LIMIT(ClientRequest &incoming, TimeStamp now) {
  trades_buffer.clear();
  orderbook.match(incoming,
                  trades_buffer); // incoming must be passed by reference.
  if (incoming.new_order.quantity > 0) {
    orderbook.add(incoming); // Some quantity was left so we add to order book.
  }
  for (auto [trade, resting] : trades_buffer) {
    trade.time_stamp = now;
    logger.logTrade(trade, resting, incoming, trade.quantity);
  }
}

template <TachyonConfig config>
void Engine<config>::handle_GTC_MARKET(ClientRequest &incoming) {
  logger.logInvalidOrder(incoming);
}

template <TachyonConfig config>
void Engine<config>::handle_IOC_LIMIT(ClientRequest &incoming, TimeStamp now) {
  trades_buffer.clear();
  orderbook.match(incoming, trades_buffer);
  // NOTE: since this is IOC order, we do NOT pass add it irrespective of
  // remaining quantity.
  for (auto [trade, resting] : trades_buffer) {
    trade.time_stamp = now;
    logger.logTrade(trade, resting, incoming, trade.quantity);
  }
}

template <TachyonConfig config>
void Engine<config>::handle_IOC_MARKET(ClientRequest &incoming, TimeStamp now) {
  if (incoming.new_order.side == Side::ASK) { // We set price to zero.
    incoming.new_order.price =
        (CLIENT_BASE_PRICE) +
        (CLIENT_PRICE_DISTRIB_MIN); // price distrib min is -ve
  } else if (incoming.new_order.side == Side::BID) {
    incoming.new_order.price = (CLIENT_BASE_PRICE) + (CLIENT_PRICE_DISTRIB_MAX);
  }
  trades_buffer.clear();
  orderbook.match(incoming, trades_buffer);
  // NOTE: since this is IOC order, we do NOT pass add it irrespective of
  // remaining quantity.
  for (auto [trade, resting] : trades_buffer) {
    trade.time_stamp = now;
    logger.logTrade(trade, resting, incoming, trade.quantity);
  }
}

// TODO: right now the execution reports logging is not correct.
// Takers have zero remaining quantity. this is because of reference at end.
// pass trade quantity and init quanity or their difference.
// However actual trades are correct.

template <TachyonConfig config> void Engine<config>::handleEvents() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  ClientRequest incoming;

  uint64_t processed_events_count = 0;
  // NOTE: Only GTC LIMIT handlers exist now.
  while (keep_running.load(std::memory_order_relaxed)) {
    if (event_queue.try_pop(incoming)) {
      TimeStamp now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
      // printEvent(incoming); // For debugging only!
      processed_events.push(incoming);
      processed_events_count++;
      if (processed_events_count % MAX_PROCESSED_EVENTS_SIZE == 0) {
        std::cout << "Events processed: " << processed_events_count << "\n";
        std::cout << "Orderbook Size: "
                  << orderbook.size_asks() + orderbook.size_bids() << "\n";
        std::cout << "Event queue size: " << event_queue.size() << "\n";
      }
      if (incoming.type == RequestType::New) {
        // For now, we assume correct price and quantity. So every order will
        // be accepted.
        logger.logNewOrder(incoming);
        if (incoming.new_order.tif == TimeInForce::GTC) {
          if (incoming.new_order.order_type == OrderType::LIMIT) {
            handle_GTC_LIMIT(incoming, now);
          } else if (incoming.new_order.order_type == OrderType::MARKET) {
            // Invalid order type. Handle properly.
            handle_GTC_MARKET(incoming);
          }
        } else if (incoming.new_order.tif == TimeInForce::IOC) {
          if (incoming.new_order.order_type == OrderType::LIMIT) {
            handle_IOC_LIMIT(incoming, now);
          } else if (incoming.new_order.order_type == OrderType::MARKET) {
            handle_IOC_MARKET(incoming, now);
          }
        }
      } else if (incoming.type == RequestType::Cancel) {
        ClientRequest to_cancel;
        if (orderbook.cancelOrder(incoming.order_id_to_cancel, to_cancel)) {
          logger.logCancelOrder(to_cancel);
        } else {
          logger.logNotFound(incoming);
        }
      }
    }
  }
}

template class Engine<my_config>;
