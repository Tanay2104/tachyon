#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <cstdint>
#include <vector>

#include "containers/lock_queue.hpp"
#include "orderbook.hpp"
#include "types.hpp"

class Engine {
 private:
  static const uint16_t MAX_PROCESSED_EVENTS_SIZE = 10000;
  static const uint16_t MAX_TRADE_BUFFER_SIZE = 1000;

  threadsafe::stl_queue<ClientRequest>& event_queue;
  // NOTE: we should clear the following queues later.
  threadsafe::stl_queue<Trade>& trades_queue;
  threadsafe::stl_queue<ExecutionReport>& execution_reports;
  threadsafe::stl_queue<ClientRequest> processed_events;
  std::vector<std::pair<Trade, ClientRequest>> trades_buffer;
  OrderBook& orderbook;

  // Helper functions for logging.
  void logNotFound(ClientRequest incoming);
  void logSelfTrade(ClientRequest incoming);    // User tried self trade
  void logNewOrder(ClientRequest incoming);     // New order logged
  void logCancelOrder(ClientRequest incoming);  // Cancellation successful.
  void logTrade(
      ClientRequest resting, ClientRequest incoming,
      Quantity trade_quantity);  // Trade has happened, send report to both.

  //  Helper functions to handle proper routing to matching function.
  void handle_GTC_LIMIT(ClientRequest incoming);
  // GTC MARKET not offered. The below function is for handling the
  // execution report only, not matching.
  void handle_GTC_MARKET(ClientRequest incoming);
  void handle_IOC_LIMIT(ClientRequest incoming);
  void handle_IOC_MARKET(ClientRequest incoming);

  // Helper function for writing logs to disk.
  void writeLogs();

 public:
  Engine(threadsafe::stl_queue<ClientRequest>& ev_q,
         threadsafe::stl_queue<Trade>& trades_queue,
         threadsafe::stl_queue<ExecutionReport>& exec_report,
         OrderBook& orderbook);
  void handleEvents();  // runs on seperate thread.
  void writeLogsContinuous();
  ~Engine();
};

#endif
