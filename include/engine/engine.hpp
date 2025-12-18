#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <cstdint>
#include <vector>

#include "containers/lock_queue.hpp"
#include "engine/constants.hpp"
#include "orderbook.hpp"
#include "types.hpp"
#include <engine/logger.hpp>
#include <engine/requires.hpp>

template <TachyonConfig config> class Engine {
private:
  // NOTE: now these are defined in constants.hpp
  // static const uint16_t MAX_PROCESSED_EVENTS_SIZE = 10000;
  // static const uint16_t MAX_TRADE_BUFFER_SIZE = 100;

  config::EventQueue &event_queue;
  // NOTE: we should clear the following queues later.

  // config::TradesQueue &trades_queue;
  // config::ExecReportQueue &execution_reports;
  LoggerClass<config> &logger;
  config::EventQueue &processed_events;
  std::vector<std::pair<Trade, ClientRequest>> trades_buffer;

  OrderBook<config> &orderbook;
  // Helper functions for logging.

  //  Helper functions to handle proper routing to matching function.
  void handle_GTC_LIMIT(ClientRequest &incoming, TimeStamp now);
  // GTC MARKET not offered. The below function is for handling the
  // execution report only, not matching.
  void handle_GTC_MARKET(ClientRequest &incoming);
  void handle_IOC_LIMIT(ClientRequest &incoming, TimeStamp now);
  void handle_IOC_MARKET(ClientRequest &incoming, TimeStamp now);

  // Helper function for writing logs to disk.
  void writeLogs();

public:
  Engine(config::EventQueue &ev_q, config::EventQueue &prcs_events,
         OrderBook<config> &orderbook, LoggerClass<config> &lgr);
  void handleEvents(); // runs on seperate thread.
  void writeLogsContinuous();
  ~Engine();
};

#endif
