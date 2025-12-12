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
      std::vector<std::pair<Trade, ClientRequest>>& trades);

 public:
  void add(ClientRequest& incoming);
  void match(ClientRequest& incoming,
             std::vector<std::pair<Trade, ClientRequest>>& trades);
  auto cancelOrder(OrderId order_id, ClientRequest& to_cancel) -> bool;
  size_t size_asks() {
    return asks.size();
  }
  size_t size_bids() {
    return bids.size();
  }
};

#endif
