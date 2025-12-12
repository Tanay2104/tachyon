#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <vector>

#include "containers/lock_queue.hpp"
#include "orderbook.hpp"
#include "types.hpp"

class Engine {
 public:
  threadsafe::stl_queue<ClientRequest>& event_queue;

  // NOTE: we should clear the following queues later.
  threadsafe::stl_queue<Trade>& trades_queue;
  threadsafe::stl_queue<ExecutionReport>& execution_reports;
  std::vector<ClientRequest> processed_events;
  OrderBook& orderbook;

  // Helper functions for logging.
  void logSelfTrade(ClientRequest incoming);
  void logExecutionReport(ClientRequest incoming);  // For logging new orders
  void logExecutionReport(
      std::set<ClientRequest>::iterator);  // For cancelling orders.
  void logExecutionReport(ClientRequest resting, ClientRequest incoming,
                          Quantity trade_quantity);  // For trades.
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
  Engine(threadsafe::stl_queue<ClientRequest>& ev_q,
         threadsafe::stl_queue<Trade>& trades_queue,
         threadsafe::stl_queue<ExecutionReport>& exec_report,
         OrderBook& orderbook);
  void handleEvents();  // runs on seperate thread.
  ~Engine();
};

#endif
