#include "engine/engine.hpp"

#include <algorithm>
#include <chrono>

#include "containers/lock_queue.hpp"
#include "engine/types.hpp"

Engine::Engine(threadsafe::stl_queue<ClientRequest>& ev_q, OrderBook& orderbook,
               threadsafe::stl_queue<Trade>& trades_queue)
    : event_queue(ev_q), trades_queue(trades_queue), orderbook(orderbook) {}

void Engine::handle_BID_GTC_LIMIT(ClientRequest incoming) {
  ClientRequest resting;
  auto asks_it = orderbook.asks.begin();
  while (incoming.new_order.quantity > 0 && asks_it != orderbook.asks.end() &&
         asks_it->new_order.price <= incoming.new_order.price) {
    resting = *asks_it;
    orderbook.asks.erase(asks_it);
    Quantity trade_quantity =
        std::min(resting.new_order.quantity, incoming.new_order.quantity);
    // Decrease quantity from both.
    resting.new_order.quantity -= trade_quantity;
    incoming.new_order.quantity -= trade_quantity;
    // Broadcasting trade data.
    // TODO: broadcast Execution report also.
    Trade new_trade;
    new_trade.aggressor_side = Side::BID;
    new_trade.quantity = trade_quantity;
    new_trade.price = resting.new_order.price;
    new_trade.maker_order_id = resting.new_order.order_id;
    new_trade.taker_order_id = incoming.new_order.order_id;
    new_trade.time_stamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    trades_queue.push(new_trade);
    if (resting.new_order.quantity > 0) {
      orderbook.asks.insert(resting);
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
    orderbook.bids.erase(bids_it);
    Quantity trade_quantity =
        std::min(resting.new_order.quantity, incoming.new_order.quantity);
    // Decrease quantity from both.
    resting.new_order.quantity -= trade_quantity;
    incoming.new_order.quantity -= trade_quantity;
    // Broadcasting trade data.
    // TODO: broadcast Execution report also.
    Trade new_trade;
    new_trade.aggressor_side = Side::ASK;
    new_trade.quantity = trade_quantity;
    new_trade.price = incoming.new_order.price;
    new_trade.maker_order_id = resting.new_order.order_id;
    new_trade.taker_order_id = incoming.new_order.order_id;
    new_trade.time_stamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    trades_queue.push(new_trade);
    if (resting.new_order.quantity > 0) {
      orderbook.bids.insert(resting);
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

void Engine::handleEvents() {
  ClientRequest incoming;
  // NOTE: Only GTC LIMIT handlers exist now.
  while (true) {
    if (event_queue.try_pop(incoming)) {
      if (incoming.type == RequestType::New) {
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
            orderbook.bids.erase(cancel_iterator);
          } else {
            // Invalid order id. Handle properly.
            return;
          }
        }
      }
    }
  }
}
