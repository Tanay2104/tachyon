#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <set>

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

 public:
  std::set<ClientRequest, bids_cmp> bids;  // People buying stuff.
  std::set<ClientRequest, asks_cmp> asks;  // People selling stuff.
};

#endif
