#include "engine/engine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
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
  processed_events.reserve(100000);
}

Engine::~Engine() {
  std::ofstream processed_requests_file;
  processed_requests_file.open("logs/processed_events.txt", std::ios::out);
  processed_requests_file << "Processed Events by Engine\n";
  processed_requests_file << processed_events.size() << " Events Processed\n";
  for (ClientRequest event : processed_events) {
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

// WARNING: Check for seg faults becase we ignore the boolean returned by insert
// in map and only use it.
void Engine::logExecutionReport(std::set<ClientRequest>::iterator order) {
  ExecutionReport exec_report;
  exec_report.client_id = order->client_id;
  exec_report.order_id = order->new_order.order_id;
  exec_report.price = order->new_order.price;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = order->new_order.quantity;
  exec_report.type = ExecType::CANCELED;
  exec_report.side = order->new_order.side;
  execution_reports.push(exec_report);
}
void Engine::logExecutionReport(ClientRequest incoming) {
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

void Engine::logExecutionReport(ClientRequest resting, ClientRequest incoming,
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
void Engine::handle_BID_GTC_LIMIT(ClientRequest incoming) {
  ClientRequest resting;
  auto asks_it = orderbook.asks.begin();
  while (incoming.new_order.quantity > 0 && asks_it != orderbook.asks.end() &&
         asks_it->new_order.price <= incoming.new_order.price) {
    resting = *asks_it;
    if (resting.client_id == incoming.client_id) {
      logSelfTrade(incoming);
      ++asks_it;
      continue;  // No trade possible between self. Issue execution report.
    }

    asks_it =
        orderbook.asks.erase(asks_it);  // this points to the NEXT iterator.
    Quantity trade_quantity =
        std::min(resting.new_order.quantity, incoming.new_order.quantity);
    // Decrease quantity from both.
    resting.new_order.quantity -= trade_quantity;
    incoming.new_order.quantity -= trade_quantity;
    // Broadcasting trade data.
    // TODO: Maybe add constructors if they look cleaner..
    Trade new_trade;
    new_trade.aggressor_side = Side::BID;
    new_trade.quantity = trade_quantity;
    new_trade.price = resting.new_order.price;
    new_trade.maker_order_id = resting.new_order.order_id;
    new_trade.taker_order_id = incoming.new_order.order_id;
    new_trade.time_stamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    trades_queue.push(new_trade);

    logExecutionReport(resting, incoming, trade_quantity);

    if (resting.new_order.quantity > 0) {
      asks_it = orderbook.asks.insert(resting)
                    .first;  // if some quantity is left we should set it here.
                             // However it doesnt matter actually because
                             // incoming has zero remaining quantity.
    }
  }
  if (incoming.new_order.quantity > 0) {
    orderbook.bids.insert(incoming);
  }
}

void Engine::handle_BID_GTC_MARKET(ClientRequest incoming) {
  incoming.order_id_to_cancel++;  // DUMMY
}

void Engine::handle_BID_IOC_LIMIT(ClientRequest incoming) {
  incoming.order_id_to_cancel++;
}

void Engine::handle_BID_IOC_MARKET(ClientRequest incoming) {
  incoming.order_id_to_cancel++;
}
void Engine::handle_ASK_GTC_LIMIT(ClientRequest incoming) {
  ClientRequest resting;
  auto bids_it = orderbook.bids.begin();
  while (incoming.new_order.quantity > 0 && bids_it != orderbook.bids.end() &&
         bids_it->new_order.price >= incoming.new_order.price) {
    resting = *bids_it;
    if (resting.client_id == incoming.client_id) {
      logSelfTrade(incoming);
      ++bids_it;
      continue;  // No trade possible between self. Issue execution report.
    }
    bids_it = orderbook.bids.erase(bids_it);
    Quantity trade_quantity =
        std::min(resting.new_order.quantity, incoming.new_order.quantity);
    // Decrease quantity from both.
    resting.new_order.quantity -= trade_quantity;
    incoming.new_order.quantity -= trade_quantity;
    // Broadcasting trade data.
    // TODO: broadcast Execution report als

    Trade new_trade;
    new_trade.aggressor_side = Side::ASK;
    new_trade.quantity = trade_quantity;
    new_trade.price =
        resting.new_order
            .price;  // NOTE: The resting order always sets the price.
    new_trade.maker_order_id = resting.new_order.order_id;
    new_trade.taker_order_id = incoming.new_order.order_id;
    new_trade.time_stamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    trades_queue.push(new_trade);
    logExecutionReport(resting, incoming, trade_quantity);
    if (resting.new_order.quantity > 0) {
      bids_it = orderbook.bids.insert(resting).first;
    }
  }
  if (incoming.new_order.quantity > 0) {
    orderbook.asks.insert(incoming);
  }
}

void Engine::handle_ASK_GTC_MARKET(ClientRequest incoming) {
  incoming.order_id_to_cancel++;  // DUMMY!
}
void Engine::handle_ASK_IOC_LIMIT(ClientRequest incoming) {
  incoming.order_id_to_cancel++;
}
void Engine::handle_ASK_IOC_MARKET(ClientRequest incoming) {
  incoming.order_id_to_cancel++;
}

// For debugging ONLY!!!
void printEvent(ClientRequest incoming) {
  std::cout << "Incoming client id: " << incoming.client_id << std::endl;
  if (incoming.type == RequestType::New) {
    std::cout << "Incoming request type: " << "NEW" << std::endl;
    std::cout << "Incoming order id: " << incoming.new_order.order_id
              << std::endl;
    std::cout << "Incoming price: " << incoming.new_order.price << std::endl;
    std::cout << "Incoming quantity: " << incoming.new_order.quantity
              << std::endl;
    std::cout << "Incoming type: " << "LIMIT" << std::endl;
    std::cout << "Incoming side: "
              << (incoming.new_order.side == Side::ASK ? "ASK" : "BID")
              << std::endl;
    std::cout << "Incoming TIF: " << "GTC" << std::endl;
    std::cout << "---" << std::endl;
  } else {
    std::cout << "Incoming request type: " << "CANCEL" << std::endl;
    std::cout << "Incoming order id: " << incoming.order_id_to_cancel
              << std::endl;
  }
}
void Engine::handleEvents() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  ClientRequest incoming;
  // NOTE: Only GTC LIMIT handlers exist now.
  while (keep_running.load(std::memory_order_relaxed)) {
    if (event_queue.try_pop(incoming)) {
      // printEvent(incoming); // For debugging only!
      processed_events.push_back(incoming);
      if (processed_events.size() % 1000 == 0) {
        std::cout << processed_events.size() << " events processed.\n";
      }
      if (incoming.type == RequestType::New) {
        // For now, we assume correct price and quantity. So every order will be
        // accepted.
        logExecutionReport(incoming);
        if (incoming.new_order.side == Side::BID) {
          if (incoming.new_order.tif == TimeInForce::GTC) {
            if (incoming.new_order.order_type == OrderType::LIMIT) {
              handle_BID_GTC_LIMIT(incoming);
            } else if (incoming.new_order.order_type == OrderType::MARKET) {
              // Invalid order type. Hanlde properly.
              handle_BID_GTC_MARKET(incoming);
            }
          } else if (incoming.new_order.tif == TimeInForce::IOC) {
            if (incoming.new_order.order_type == OrderType::LIMIT) {
              handle_BID_IOC_LIMIT(incoming);
            } else if (incoming.new_order.order_type == OrderType::MARKET) {
              handle_BID_IOC_MARKET(incoming);
            }
          }
        }

        else if (incoming.new_order.side == Side::ASK) {
          if (incoming.new_order.tif == TimeInForce::GTC) {
            if (incoming.new_order.order_type == OrderType::LIMIT) {
              handle_ASK_GTC_LIMIT(incoming);
            } else if (incoming.new_order.order_type == OrderType::MARKET) {
              // Invalid order type. Hanlde properly.
              handle_ASK_GTC_MARKET(incoming);
            }
          } else if (incoming.new_order.tif == TimeInForce::IOC) {
            if (incoming.new_order.order_type == OrderType::LIMIT) {
              handle_ASK_IOC_LIMIT(incoming);
            } else if (incoming.new_order.order_type == OrderType::MARKET) {
              handle_ASK_IOC_MARKET(incoming);
            }
          }
        }
      } else if (incoming.type == RequestType::Cancel) {
        // First we find in bids, then asks, else give error or something.
        OrderId to_cancel =
            incoming.order_id_to_cancel;  // idk why the find if iterator gives
                                          // errors on using dot.
        auto cancel_iterator = std::ranges::find_if(
            orderbook.asks, [to_cancel](const ClientRequest& clr) -> bool {
              return (to_cancel == clr.new_order.order_id);
            });

        if (cancel_iterator != orderbook.asks.end()) {
          // The order exists in the Asks map.
          logExecutionReport(cancel_iterator);
          orderbook.asks.erase(cancel_iterator);
          // TODO: Add execution report and return.
        } else {
          // It doen't exist in asks.
          cancel_iterator = std::ranges::find_if(
              orderbook.bids, [to_cancel](const ClientRequest& clr) -> bool {
                return (to_cancel == clr.new_order.order_id);
              });
          if (cancel_iterator != orderbook.bids.end()) {
            // It was a bid order
            logExecutionReport(cancel_iterator);
            orderbook.bids.erase(cancel_iterator);
          } else {
            ExecutionReport exec_report;
            exec_report.client_id = incoming.client_id;
            exec_report.type = ExecType::REJECTED;
            exec_report.price = 0;
            exec_report.order_id = incoming.order_id_to_cancel;
            exec_report.last_quantity = 0;
            exec_report.remaining_quantity = 0;
            exec_report.reason = RejectReason::ORDER_NOT_FOUND;
            execution_reports.push(exec_report);
          }
        }
      }
    }
  }
}
