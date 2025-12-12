#include "engine/engine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <thread>

#include "containers/lock_queue.hpp"
#include "engine/types.hpp"

extern std::atomic<bool> start_exchange;
extern std::atomic<bool> keep_running;

Engine::Engine(threadsafe::stl_queue<ClientRequest>& ev_q,
               threadsafe::stl_queue<Trade>& trades_queue,
               threadsafe::stl_queue<ExecutionReport>& exec_report,
               OrderBook& orderbook)
    : event_queue(ev_q),
      trades_queue(trades_queue),
      execution_reports(exec_report),
      orderbook(orderbook) {
  trades_buffer.reserve(MAX_TRADE_BUFFER_SIZE);
  std::ofstream processed_requests_file;
  processed_requests_file.open("logs/processed_events.txt", std::ios ::out);
  processed_requests_file << "Processed Events by Engine\n";
  processed_requests_file.close();
}

Engine::~Engine() { writeLogs(); }

void Engine::writeLogs() {
  std::ofstream processed_requests_file;
  processed_requests_file.open("logs/processed_events.txt", std::ios::app);
  ClientRequest event;
  uint32_t before_size = processed_events.size();
  for (uint32_t i = 0; i < before_size;
       i++) {  // At least MAX PROCESSED events must be there.
    processed_events.try_pop(event);
    processed_requests_file << "Client " << event.client_id << ": ";
    if (event.type == RequestType::New) {
      processed_requests_file
          << "ORDER ID " << event.new_order.order_id << " "
          << (event.new_order.side == Side::BID ? "BUY " : "SELL ")
          << event.new_order.quantity << " @ " << event.new_order.price << " "
          << (event.new_order.order_type == OrderType::LIMIT ? "LIMIT "
                                                             : "MARKET ")
          << (event.new_order.tif == TimeInForce::GTC ? "GTC " : "IOC ")
          << "TIMESTAMP-" << event.time_stamp << "\n";
    } else if (event.type == RequestType::Cancel) {
      processed_requests_file << "CANCEL " << " ORDER ID "
                              << event.order_id_to_cancel << " TIMESTAMP-"
                              << event.time_stamp << "\n";
    }
  }
  processed_requests_file.close();
}

void Engine::writeLogsContinuous() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  while (keep_running.load(std::memory_order_relaxed)) {
    if (processed_events.size() >= MAX_PROCESSED_EVENTS_SIZE) {
      writeLogs();
    }
  }
}
// WARNING: Check for seg faults becase we ignore the boolean returned by insert
// in map and only use it.
void Engine::logCancelOrder(ClientRequest incoming) {
  ExecutionReport exec_report;
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = incoming.new_order.price;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::CANCELED;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);
}

void Engine::logNotFound(ClientRequest incoming) {
  ExecutionReport exec_report;
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.order_id_to_cancel;
  exec_report.price = 0;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = 0;
  exec_report.type = ExecType::REJECTED;
  exec_report.reason = RejectReason::ORDER_NOT_FOUND;
  execution_reports.push(exec_report);
}
void Engine::logNewOrder(ClientRequest incoming) {
  ExecutionReport exec_report;
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = incoming.new_order.price;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::NEW;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);
}

void Engine::logTrade(ClientRequest resting, ClientRequest incoming,
                      Quantity trade_quantity) {
  ExecutionReport exec_report;

  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = resting.new_order.price;
  exec_report.last_quantity = trade_quantity;  // Quantity traded
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::TRADE;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);

  exec_report.client_id = resting.client_id;
  exec_report.order_id = resting.new_order.order_id;
  exec_report.price = resting.new_order.price;
  exec_report.last_quantity = trade_quantity;
  exec_report.remaining_quantity = resting.new_order.quantity;
  exec_report.type = ExecType::TRADE;
  exec_report.side = resting.new_order.side;
  execution_reports.push(exec_report);
}
void Engine::logSelfTrade(ClientRequest incoming) {
  ExecutionReport report;
  report.client_id = incoming.client_id;
  report.last_quantity = 0;
  report.remaining_quantity = incoming.new_order.quantity;
  report.price = incoming.new_order.price;
  report.order_id = incoming.new_order.order_id;
  report.type = ExecType::REJECTED;
  report.reason = RejectReason::SELF_TRADE;
  report.side = incoming.new_order.side;

  execution_reports.push(report);
}

void Engine::handle_GTC_LIMIT(ClientRequest incoming) {
  trades_buffer.clear();
  orderbook.match(incoming,
                  trades_buffer);  // incoming must be passed by reference.
  if (incoming.new_order.quantity > 0) {
    orderbook.add(incoming);  // Some quantity was left so we add to order book.
  }
  for (auto [trade, resting] : trades_buffer) {
    trades_queue.push(trade);  // Maybe we don't need it. Let's see.
    logTrade(resting, incoming, trade.quantity);
  }
}

void Engine::handle_GTC_MARKET(ClientRequest incoming) {
  incoming.order_id_to_cancel++;  // DUMMY
}

void Engine::handle_IOC_LIMIT(ClientRequest incoming) {
  trades_buffer.clear();
  orderbook.match(incoming, trades_buffer);
  // NOTE: since this is IOC order, we do NOT pass add it irrespective of
  // remaining quantity.
  for (auto [trade, resting] : trades_buffer) {
    trades_queue.push(trade);  // Maybe we don't need it. Let's see.
    logTrade(resting, incoming, trade.quantity);
  }
}

void Engine::handle_IOC_MARKET(ClientRequest incoming) {
  incoming.order_id_to_cancel++;
}

void Engine::handleEvents() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  ClientRequest incoming;

  uint64_t processed_events_count = 0;
  // NOTE: Only GTC LIMIT handlers exist now.
  while (keep_running.load(std::memory_order_relaxed)) {
    if (event_queue.try_pop(incoming)) {
      // printEvent(incoming); // For debugging only!
      processed_events.push(incoming);
      processed_events_count++;
      if (processed_events_count % 10000 == 0) {
        std::cout << processed_events_count << " events processed.\n";
      }
      if (incoming.type == RequestType::New) {
        // For now, we assume correct price and quantity. So every order will be
        // accepted.
        logNewOrder(incoming);
        if (incoming.new_order.tif == TimeInForce::GTC) {
          if (incoming.new_order.order_type == OrderType::LIMIT) {
            handle_GTC_LIMIT(incoming);
          } else if (incoming.new_order.order_type == OrderType::MARKET) {
            // Invalid order type. Handle properly.
            handle_GTC_MARKET(incoming);
          }
        } else if (incoming.new_order.tif == TimeInForce::IOC) {
          if (incoming.new_order.order_type == OrderType::LIMIT) {
            handle_IOC_LIMIT(incoming);
          } else if (incoming.new_order.order_type == OrderType::MARKET) {
            handle_IOC_MARKET(incoming);
          }
        }
      } else if (incoming.type == RequestType::Cancel) {
        ClientRequest to_cancel;
        if (orderbook.cancelOrder(incoming.order_id_to_cancel, to_cancel)) {
          logCancelOrder(to_cancel);
        } else {
          logNotFound(incoming);
        }
      }
    }
  }
}
