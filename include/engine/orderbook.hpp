#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <set>
#include <vector>

#include "engine/types.hpp"

class OrderBook {
 private:
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

  std::set<ClientRequest, bids_cmp> bids;  // People buying stuff.
  std::set<ClientRequest, asks_cmp> asks;  // People selling stuff.

  template <typename BookType, typename CompareFunc>
  void matchImplementation(
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
  }

 public:
  void add(ClientRequest& incoming);
  void match(ClientRequest& incoming,
             std::vector<std::pair<Trade, ClientRequest>>& trades);
  auto cancelOrder(OrderId order_id, ClientRequest& to_cancel) -> bool;
  size_t size_asks() { return asks.size(); }
  size_t size_bids() { return bids.size(); }
};

#endif
