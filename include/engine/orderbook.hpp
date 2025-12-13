#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstdint>
#include <deque>
#include <set>
#include <unordered_map>
#include <vector>

#include "containers/intrusive_list.hpp"
#include "engine/constants.hpp"
#include "engine/types.hpp"
class OrderBook {
 private:
  // TODO: shift to a custom block allocater.
  struct OrderSlot {  // NOTE: make sure ABA problem doesn't occur.

    ClientRequest clr;
    bool is_active = false;
  };
  std::deque<OrderSlot> arena;      // Our active orders live here.
  std::vector<uint32_t> free_list;  // Acts as stack.

  auto allocateSlot(const ClientRequest& incoming) -> uint32_t;
  void freeSlot(uint32_t idx);
  struct bids_cmp {
    auto operator()(const ClientRequest& order_a,
                    const ClientRequest& order_b) const -> bool {
      if (order_a.new_order.price == order_b.new_order.price) {
        return order_a.time_stamp < order_b.time_stamp;
      }
      return (order_a.new_order.price >
              order_b.new_order.price);  // Big with higher price comes first.
    }
  };

  struct asks_cmp {
    auto operator()(const ClientRequest& order_a,
                    const ClientRequest& order_b) const -> bool {
      if (order_a.new_order.price == order_b.new_order.price) {
        return order_a.time_stamp < order_b.time_stamp;
      }
      return (order_a.new_order.price <
              order_b.new_order.price);  // Big with higher price comes first.
    }
  };

  // std::set<ClientRequest, bids_cmp> bids;  // People buying stuff.
  // std::set<ClientRequest, asks_cmp> asks;  // People selling stuff.
  // std::deque<ClientRequest> arena;  // This is where the orders live.
  std::unordered_map<OrderId, uint32_t> arena_idx;
  std::unordered_map<
      OrderId, std::tuple<Side, Price,
                          intrusive_list<ClientRequest>::ListIterator<false>>>
      list_idx;
  std::vector<intrusive_list<ClientRequest>> bids;
  std::vector<intrusive_list<ClientRequest>> asks;
  template <typename BookType, typename CompareFunc>
  void matchImplementation(
      ClientRequest& incoming, BookType& book, CompareFunc priceCrosses,
      std::vector<std::pair<Trade, ClientRequest>>& trades) {
    Price book_price = 0;
    if (incoming.new_order.side == Side::BID) {
      while (book_price < book.size() && book[book_price].size() == 0) {
        book_price++;
        // TODO: we can add a member variable to orderbook instead of doing
        // this. Basically find a better way to start the matching process.
      }
      if (book_price == book.size()) {
        return;
      }
    } else if (incoming.new_order.side == Side::ASK) {
      book_price = book.size() - 1;
      while (book_price >= 0 && book[book_price].size() == 0) {
        book_price--;
      }
      if (book_price == -1) {
        return;  // Underflow will happen.
      }
    }
    auto book_it = book[book_price].begin();

    while (incoming.new_order.quantity > 0 && book_price < book.size() &&
           book_price >= 0) {
      while (incoming.new_order.quantity > 0 &&
             book_it != book[book_price].end() &&
             priceCrosses(book_it->new_order.price, incoming.new_order.price)) {
        // CRITICAL: Book it id state getting corrupted sometimes!
        if (book_it->client_id == incoming.client_id) {
          // TODO: add something for broadcasting errors.

          ++book_it;
          continue;
        }
        // book_it = book[book_price].remove(book_it);
        Quantity trade_quantity =
            std::min(book_it->new_order.quantity, incoming.new_order.quantity);
        // Decrease quantity from both.
        book_it->new_order.quantity -= trade_quantity;
        incoming.new_order.quantity -= trade_quantity;
        // TODO: maybe add constructors if they look cleaner?
        Trade new_trade;
        new_trade.aggressor_side = incoming.new_order.side;
        new_trade.quantity = trade_quantity;
        new_trade.price = book_it->new_order.price;
        new_trade.maker_order_id = book_it->new_order.order_id;
        new_trade.taker_order_id = incoming.new_order.order_id;
        new_trade.time_stamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        trades.push_back({new_trade, *book_it});
        // TODO: log execution reports too.
        if (book_it->new_order.quantity == 0) {
          // arena.erase(arena_idx[book_it->new_order.order_id]); // deleting
          // old elements from arena.
          // TODO: erasing from arena has linear complexity. Do something.
          freeSlot(arena_idx[book_it->new_order.order_id]);
          arena_idx.erase(book_it->new_order.order_id);
          book_it =
              book[book_price].remove(book_it);  // remove finished orders.

        } else {
          book_it++;  // Do we really need this?
        }
      }
      // Now orders at that price level are finished.
      if (incoming.new_order.side == Side::ASK) {
        book_price--;
      } else {
        book_price++;
      }
      if (book_price == std::numeric_limits<Price>::max() ||
          book_price >= book.size()) {
        // -1 didn't work because of underflow.
        break;  // No further orders to sweep.
      }
      book_it = book[book_price].begin();
    }
  }

 public:
  OrderBook()
      : bids(std::vector<intrusive_list<ClientRequest>>(
            uint32_t((CLIENT_PRICE_DISTRIB_MAX) - (CLIENT_PRICE_DISTRIB_MIN)) +
            1)),
        asks(std::vector<intrusive_list<ClientRequest>>(
            uint32_t((CLIENT_PRICE_DISTRIB_MAX) - (CLIENT_PRICE_DISTRIB_MIN)) +
            1))

  {}
  void add(ClientRequest& incoming);
  void match(ClientRequest& incoming,
             std::vector<std::pair<Trade, ClientRequest>>& trades);
  auto cancelOrder(OrderId order_id, ClientRequest& to_cancel) -> bool;
  auto size_asks() -> size_t;
  auto size_bids() -> size_t;
};

#endif
