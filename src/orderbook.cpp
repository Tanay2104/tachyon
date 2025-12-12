#include "engine/orderbook.hpp"

#include <algorithm>
#include <new>
#include <ranges>
#include <vector>

#include "engine/client.hpp"
#include "engine/types.hpp"

template <typename BookType, typename CompareFunc>
void OrderBook::matchImplementation(
    ClientRequest& incoming, BookType& book, CompareFunc priceCrosses,
    std::vector<std::pair<Trade, ClientRequest>>& trades) {
  ClientRequest resting;
  auto book_it = book.begin();
  while (incoming.new_order.quantity > 0 && book_it != book.end() &&
         priceCrosses(book_it->new_order.price, incoming.new_order.price)) {
    resting = *book_it;
    if (resting.client_id == incoming.client_id) {
      // TODO: add something for broadcasting errors.
      ++book_it;
      continue;
    }
    book_it = book.erase(book_it);
    Quantity trade_quantity =
        std::min(resting.new_order.quantity, incoming.new_order.quantity);
    // Decrease quantity from both.
    resting.new_order.quantity -= trade_quantity;
    incoming.new_order.quantity -= trade_quantity;
    // TODO: maybe add constructors if they look cleaner?
    Trade new_trade;
    new_trade.aggressor_side = incoming.new_order.side;
    new_trade.quantity = trade_quantity;
    new_trade.price = resting.new_order.price;
    new_trade.maker_order_id = resting.new_order.order_id;
    new_trade.taker_order_id = incoming.new_order.order_id;
    new_trade.time_stamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    trades.emplace_back(new_trade, resting);
    // TODO: log execution reports too.
    if (resting.new_order.quantity > 0) {
      book_it = book.insert(resting).first;
      // if some quantity is left we should set it here.
      // However it doesnt matter actually because
      // incoming has zero remaining quantity.
    };
  }
  if (incoming.new_order.quantity > 0) {
    // NOTE: should we send an execution report as a new order or silently add?
    add(incoming);
  }
}

void OrderBook::add(ClientRequest& incoming) {
  // TODO: Make it return boolean for invalid orders?
  if (incoming.new_order.side == Side::BID) {
    bids.insert(incoming);
  } else {
    asks.insert(incoming);
  }
}

void OrderBook::match(ClientRequest& incoming,
                      std::vector<std::pair<Trade, ClientRequest>>& trades) {
  if (incoming.new_order.side == Side::BID) {
    matchImplementation(
        incoming, asks,
        [](Price p_sell, Price p_buy) -> bool { return (p_buy >= p_sell); },
        trades);
  } else {
    matchImplementation(
        incoming, bids,
        [](Price p_buy, Price p_sell) -> bool { return (p_buy >= p_sell); },
        trades);
  }
}

auto OrderBook::cancelOrder(OrderId order_id, ClientRequest& to_cancel)
    -> bool {
  auto cancel_iterator =
      std::ranges::find_if(asks, [order_id](const ClientRequest& clr) -> bool {
        return (order_id == clr.new_order.order_id);
      });
  // Order exists in the Asks map
  if (cancel_iterator != asks.end()) {
    to_cancel = *cancel_iterator;
    asks.erase(cancel_iterator);
    return true;
  }
  //  It doesnt exist in Asks
  cancel_iterator =
      std::ranges::find_if(bids, [order_id](const ClientRequest& clr) -> bool {
        return (order_id == clr.new_order.order_id);
      });

  if (cancel_iterator != bids.end()) {
    // It was bid order.
    to_cancel = *cancel_iterator;
    bids.erase(cancel_iterator);
    return true;
  }
  // Order not found!
  return false;
}
