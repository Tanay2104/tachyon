#ifndef EXCHANGE_HPP
#define EXCHANGE_HPP

#include <chrono>
#include <network/tcpserver.hpp>
#include <thread>

#include "containers/lock_queue.hpp"
#include "engine/engine.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"

class Exchange {
 private:
  // Various queues necessary.
  threadsafe::stl_queue<ClientRequest> event_queue;
  threadsafe::stl_queue<Trade> trades_queue;
  threadsafe::stl_queue<ExecutionReport> execution_report;
  // Key components.
  OrderBook orderbook;
  // GateWay gateway;
  Engine engine;
  TcpServer tcpserver;

  // Clients.
  // int num_clients = 0;
  // std::deque<std::unique_ptr<Client>> clients;
  // std::deque<std::thread> client_threads;

  // Threads.
  std::thread engine_event_handler;
  std::thread engine_event_log_writer;
  std::thread execution_report_dispatcher;
  std::thread trades_log_writer;
  std::thread tcpserver_recieve;

  // Start time of Exchange.
  std::chrono::steady_clock::time_point start;

  // Functions to write to file.
  void writeTrades();
  void writeTradesContinuous();

 public:
  Exchange();
  ~Exchange();
  void init();
  void stop();
  // void addClients(int num = NUM_DEFAULT_CLIENTS);
  void run();
};

#endif
