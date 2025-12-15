#include "engine/orderbook.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "engine/client.hpp"
#include "engine/types.hpp"

void OrderBook::add(ClientRequest& incoming) {
  // TODO: Make it return boolean for invalid orders?
  Price book_price =
      incoming.new_order.price - CLIENT_BASE_PRICE -
      (CLIENT_PRICE_DISTRIB_MIN);  // price_distrib_min is negative.
  // NOTE: we use an arena otheriwse objects are destroyed.
  if (book_price < 0 || book_price >= bids.size()) {
    // TODO: make proper logging instead of throwing.
    throw std::out_of_range("Price invalid");
  }
  allocateSlot(incoming);
  if (incoming.new_order.side == Side::BID) {
    bids[book_price].push_back(
        arena[arena_idx.get(incoming.new_order.order_id)].clr);
    intrusive_list<ClientRequest>::iterator it = bids[book_price].end();
    it--;
    list_idx.insert(incoming.new_order.order_id, {Side::BID, book_price, it});
  } else {
    asks[book_price].push_back(
        arena[arena_idx.get(incoming.new_order.order_id)].clr);

    intrusive_list<ClientRequest>::iterator it = asks[book_price].end();
    it--;
    list_idx.insert(incoming.new_order.order_id, {Side::ASK, book_price, it});
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
  auto [side, price, it] = list_idx.get(order_id);

  if (side == Side::BID) {
    if ((bids[price].size() == 0) || (it == bids[price].end())) {
      // Corrupted state. idx exists but not in list.
      list_idx.remove(order_id);
      return false;
    }
    to_cancel = *it;
    bids[price].remove(it);
    freeSlot(arena_idx.get(order_id));  // Free a slot.
    arena_idx.remove(order_id);         // Also erase it's map.
    list_idx.remove(order_id);
    return true;
  }
  if (side == Side::ASK) {
    if ((asks[price].size() == 0) || (it == asks[price].end())) {
      // Corrupted state. idx exists but not in list.
      list_idx.remove(order_id);
      return false;
    }

    to_cancel = *it;
    asks[price].remove(it);
    freeSlot(arena_idx.get(order_id));  // Free a slot.
    arena_idx.remove(order_id);         // Also erase it's map.
    list_idx.remove(order_id);
    return true;
  }
  return false;
}

auto OrderBook::allocateSlot(const ClientRequest& incoming) -> uint32_t {
  uint32_t idx;
  if (!free_list.empty()) {
    // recycle an old block.
    idx = free_list.back();
    free_list.pop_back();
  } else {
    // create new slot.
    idx = arena.size();
    arena.emplace_back();
  }
  arena[idx].clr = incoming;
  arena[idx].is_active = true;
  arena_idx.insert(incoming.new_order.order_id, idx);

  return idx;
}

void OrderBook::freeSlot(uint32_t idx) {
  assert(arena[idx].is_active);
  arena[idx].is_active = false;
  free_list.push_back(idx);
}

// Hopefully this function is not called.
auto OrderBook::size_asks() -> size_t {
  size_t size = 0;
  for (size_t i = 0; i < asks.size(); i++) {
    size += asks[i].size();
  }
  return size;
}

auto OrderBook::size_bids() -> size_t {
  size_t size = 0;
  for (size_t i = 0; i < bids.size(); i++) {
    size += bids[i].size();
  }
  return size;
}
