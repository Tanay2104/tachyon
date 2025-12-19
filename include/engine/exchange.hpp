#ifndef EXCHANGE_HPP
#define EXCHANGE_HPP

#include <chrono>
#include <network/tcpserver.hpp>
#include <thread>

#include "containers/intrusive_list.hpp"
#include "containers/lock_queue.hpp"
#include "engine/concepts.hpp"
#include "engine/engine.hpp"
#include "engine/logger.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"

template <TachyonConfig config> class Exchange {
private:
  // Various queues necessary.
  config::EventQueue event_queue;
  config::EventQueue processed_events;
  config::TradesQueue trades_queue;
  config::ExecReportQueue execution_report;
  // Key components.
  OrderBook<config> orderbook;
  LoggerClass<config> logger;
  Engine<config> engine;
  TcpServer<config> tcpserver;

  // Threads.
  std::thread engine_event_handler;
  std::thread engine_event_log_writer;
  std::thread execution_report_dispatcher;
  std::thread trades_log_writer;
  std::thread tcpserver_recieve;

  // Start time of Exchange.
  std::chrono::steady_clock::time_point start;

public:
  Exchange();
  ~Exchange();
  void init();
  void stop();
  void run();
};

#endif
