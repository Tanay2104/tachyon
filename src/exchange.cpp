#include "engine/exchange.hpp"

#include <malloc.h>

#include <chrono>
#include <thread>

#include "engine/gateway.hpp"

std::atomic<bool> start_exchange(false);
extern std ::atomic<bool> keep_running;
Exchange::Exchange()
    : gateway(event_queue, execution_report),
      engine(event_queue, trades_queue, execution_report, orderbook) {}

void Exchange::init() {
  // Should automatically use std::move.
  engine_event_handler = std::thread(&Engine::handleEvents, &engine);
  engine_event_log_writer = std::thread(&Engine::writeLogsContinuous, &engine);
  execution_report_dispatcher = std::thread(&GateWay::dispatcher, &gateway);
  trades_log_writer = std::thread(&Exchange::writeTradesContinuous, this);

  // Getting ready or later logging.
  std::ofstream file;
  // Writing trades.
  file.open("logs/processed_trades.txt", std::ios::out);
  file << "Processed Trades\n";
  file.close();
  std::cout << "Exchange initialised\n";
}
void Exchange::addClients(int num) {
  num_clients += num;
  for (int i = 1; i <= num; i++) {
    clients.emplace_back(Client(i, gateway, trades_queue));
    client_threads.emplace_back(&Client::run, &clients.back());
    gateway.addClient(i, &clients.back());
  }
}

void Exchange::writeTrades() {
  std::ofstream file;
  // Writing trades.
  file.open("logs/processed_trades.txt", std::ios::app);
  Trade trade;
  for (uint32_t i = 0; i < MAX_TRADES_QUEUE_SIZE; i++) {
    if (trades_queue.try_pop(  // This should succeed.
            trade)) {          // Note: trades qeueue will automatically
                               // be cleared by this mechanism.
      file << "MAKER: " << trade.maker_order_id
           << " TAKER: " << trade.taker_order_id << " " << trade.quantity
           << " @ " << trade.price << " TIMESTAMP-" << trade.time_stamp << "\n";
    } else {
      std::this_thread::yield();
    }
  }

  file.close();
}

void Exchange::writeTradesContinuous() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  while (keep_running.load(std::memory_order_relaxed)) {
    if (trades_queue.size() == MAX_TRADES_QUEUE_SIZE) {
      writeTrades();
      malloc_trim(0);
    } else {
      std::this_thread::yield();
    }
  }
}

void Exchange::run() {
  start_exchange.store(true, std::memory_order_release);
  std::cout << "Exchange has opened\n";
  start = std::chrono::steady_clock::now();
}

Exchange::~Exchange() {
  auto end = std::chrono::steady_clock::now();
  std::cout << "Exchange ran for "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << "ms\n";
  writeTrades();
}

void Exchange::stop() {
  keep_running.store(false);
  std::cout << "Exchange has closed\n";
  for (auto& client_order : client_threads) {
    client_order.join();  // Joining the clients.
  }
  engine_event_log_writer.join();
  engine_event_handler.join();
  execution_report_dispatcher.join();
  trades_log_writer.join();
}
