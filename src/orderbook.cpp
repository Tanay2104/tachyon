#include "engine/orderbook.hpp"
#include "my_config.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "engine/concepts.hpp"
#include "engine/types.hpp"

template <TachyonConfig config>
void OrderBook<config>::add(ClientRequest &incoming) {
  // TODO: Make it return boolean for invalid orders?
  Price book_price =
      incoming.new_order.price - CLIENT_BASE_PRICE -
      (CLIENT_PRICE_DISTRIB_MIN); // price_distrib_min is negative.
  // NOTE: we use an arena otheriwse objects are destroyed.
  if (book_price < 0 || book_price >= bids.size()) {
    // TODO: make proper logging instead of throwing.
    throw std::out_of_range("Price invalid");
  }

  uint32_t allotted_index = arena.allocateSlot(incoming);
  OrderId order_id =
      incoming.new_order.order_id; // NOTE: directly binding gives some error.
  arena_idx.insert({order_id, allotted_index});

  if (incoming.new_order.side == Side::BID) {
    bids[book_price].push_back(
        arena[arena_idx.at(incoming.new_order.order_id)].clr);
    intrusive_list<ClientRequest>::iterator it = bids[book_price].end();
    it--;
    list_idx.insert({order_id, {Side::BID, book_price, it}});
  } else {
    asks[book_price].push_back(
        arena[arena_idx.at(incoming.new_order.order_id)].clr);

    intrusive_list<ClientRequest>::iterator it = asks[book_price].end();
    it--;
    list_idx.insert({order_id, {Side::ASK, book_price, it}});
  }
}

template <TachyonConfig config>
void OrderBook<config>::match(
    ClientRequest &incoming,
    std::vector<std::pair<Trade, ClientRequest>> &trades) {
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

template <TachyonConfig config>
auto OrderBook<config>::cancelOrder(OrderId order_id, ClientRequest &to_cancel)
    -> bool {
  if (!list_idx.contains(order_id)) {
    return false;
  }
  auto [side, price, it] = list_idx.at(order_id);

  if (side == Side::BID) {
    if ((bids[price].size() == 0) || (it == bids[price].end())) {
      // Corrupted state. idx exists but not in list.
      list_idx.erase(order_id);
      return false;
    }
    to_cancel = *it;
    bids[price].erase(it);
    arena.freeSlot(arena_idx.at(order_id)); // Free a slot.
    arena_idx.erase(order_id);              // Also erase it's map.
    list_idx.erase(order_id);
    return true;
  }
  if (side == Side::ASK) {
    if ((asks[price].size() == 0) || (it == asks[price].end())) {
      // Corrupted state. idx exists but not in list.
      list_idx.erase(order_id);
      return false;
    }

    to_cancel = *it;
    asks[price].erase(it);
    arena.freeSlot(arena_idx.at(order_id)); // Free a slot.
    arena_idx.erase(order_id);              // Also erase it's map.
    list_idx.erase(order_id);
    return true;
  }
  return false;
}

// Hopefully this function is not called.

template <TachyonConfig config> auto OrderBook<config>::size_asks() -> size_t {
  size_t size = 0;
  for (size_t i = 0; i < asks.size(); i++) {
    size += asks[i].size();
  }
  return size;
}

template <TachyonConfig config> auto OrderBook<config>::size_bids() -> size_t {
  size_t size = 0;
  for (size_t i = 0; i < bids.size(); i++) {
    size += bids[i].size();
  }
  return size;
}

// NOTE: templated types that are to be used should be placed here.
template class OrderBook<my_config>;
