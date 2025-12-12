#include "engine/orderbook.hpp"

#include <algorithm>
#include <new>
#include <ranges>
#include <vector>

#include "engine/client.hpp"
#include "engine/types.hpp"

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
