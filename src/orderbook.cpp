#include "engine/orderbook.hpp"

#include <algorithm>
#include <new>
#include <ranges>
#include <vector>

#include "engine/client.hpp"
#include "engine/types.hpp"

void OrderBook::add(ClientRequest& incoming) {
  // TODO: Make it return boolean for invalid orders?
  Price book_price =
      incoming.new_order.price - CLIENT_BASE_PRICE -
      (CLIENT_PRICE_DISTRIB_MIN);  // price_distrib_min is negative.
  // NOTE: we use an arena otheriwse objects are destroyed.
  arena.push_back(incoming);
  arena_idx[incoming.new_order.order_id] = arena.end() - 1;
  if (incoming.new_order.side == Side::BID) {
    bids[book_price].push_back(arena.back());
    intrusive_list<ClientRequest>::iterator it = bids[book_price].end();
    it--;
    list_idx[incoming.new_order.order_id] = {Side::BID, book_price, it};
  } else {
    asks[book_price].push_back(arena.back());
    intrusive_list<ClientRequest>::iterator it = asks[book_price].end();
    it--;
    list_idx[incoming.new_order.order_id] = {Side::ASK, book_price, it};
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
  if (!list_idx.contains(order_id)) {
    return false;
  }
  auto [side, price, it] = list_idx[order_id];
  if (it == bids[price].end()) {
    return false;
  }
  else if (it == asks[price].end()) {
    return false;
  }
  else if (side == Side::BID) {
    to_cancel = *it;
    bids[price].remove(it);
    return true;
  } else if (side == Side::ASK) {
    to_cancel = *it;
    asks[price].remove(it);
    return true;
  }
  return false;
}

// Hopefully this function is not called.
size_t OrderBook::size_asks() {
  size_t size = 0;
  for (int i = 0; i < asks.size(); i++) {
    size += asks[i].size();
    std::cout << size << std::endl;
  }
}

size_t OrderBook::size_bids() {
  size_t size = 0;
  for (int i = 0; i < bids.size(); i++) {
    size += bids[i].size();
  }
}
