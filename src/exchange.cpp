#include "engine/exchange.hpp"
#include "my_config.hpp"

#include <malloc.h>

#include <chrono>
#include <fstream>
#include <thread>

#include "network/tcpserver.hpp"

std::atomic<bool> start_exchange(false);
extern std ::atomic<bool> keep_running;

template <TachyonConfig config>
Exchange<config>::Exchange()
    : logger(event_queue, execution_report, trades_queue, processed_events),
      engine(event_queue, processed_events, orderbook, logger),
      tcpserver(event_queue, execution_report) {}

template <TachyonConfig config> void Exchange<config>::init() {
  tcpserver.init("12345");
  // Should automatically use std::move.
  engine_event_handler = std::thread(&Engine<config>::handleEvents, &engine);
  engine_event_log_writer = std::thread(
      &LoggerClass<config>::writeProcessedEventsLogsContinuous, &logger);
  execution_report_dispatcher =
      std::thread(&TcpServer<config>::dispatchData, &tcpserver);
  trades_log_writer =
      std::thread(&LoggerClass<config>::writeTradeLogsContinuous, &logger);
  tcpserver_recieve = std::thread(&TcpServer<config>::receiveData, &tcpserver);

  std::cout << "Exchange initialised\n";
}

template <TachyonConfig config> void Exchange<config>::run() {
  start_exchange.store(true, std::memory_order_release);
  std::cout << "Exchange has opened\n";
  start = std::chrono::steady_clock::now();
}

template <TachyonConfig config> Exchange<config>::~Exchange() = default;

template <TachyonConfig config> void Exchange<config>::stop() {
  keep_running.store(false);
  std::cout << "Exchange has closed\n";
  /* for (auto& client_order : client_threads) {
    client_order.join();  // Joining the clients.
  } */
  engine_event_log_writer.join();
  engine_event_handler.join();
  // execution_report_dispatcher.join();
  trades_log_writer.join();
  tcpserver_recieve.join();
}

template class Exchange<my_config>;
