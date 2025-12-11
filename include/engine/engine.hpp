#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "containers/lock_queue.hpp"
#include "orderbook.hpp"
#include "types.hpp"

class Engine {
 private:
  threadsafe::stl_queue<ClientRequest>& event_queue;
  threadsafe::stl_queue<Trade>& trades_queue;

  OrderBook& orderbook;

  // Helper functions for various types of events.

  void handle_BID_GTC_LIMIT(ClientRequest incoming);
  // BID GTC MARKET not offered. The below function is for handling the
  // execution report only, not matching.

  void handle_BID_GTC_MARKET(ClientRequest incoming);
  void handle_BID_IOC_LIMIT(ClientRequest incoming);
  void handle_BID_IOC_MARKET(ClientRequest incoming);
  void handle_ASK_GTC_LIMIT(ClientRequest incoming);
  // ASK GTC MARKET not offered. The below function is for handling the
  // execution report only, not matching.
  void handle_ASK_GTC_MARKET(ClientRequest incoming);
  void handle_ASK_IOC_LIMIT(ClientRequest incoming);
  void handle_ASK_IOC_MARKET(ClientRequest incoming);

 public:
  Engine(threadsafe::stl_queue<ClientRequest>& ev_q, OrderBook& orderbook,
         threadsafe::stl_queue<Trade>& trades_queue);
  void handleEvents();  // runs on seperate thread.
};

#endif
