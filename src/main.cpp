#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <vector>

#include "containers/lock_queue.hpp"
#include "engine/client.hpp"
#include "engine/engine.hpp"
#include "engine/gateway.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"

std::atomic<bool> keep_running(true);
std::atomic<bool> start_exchange(false);
void printTrades(threadsafe::stl_queue<Trade>& trades_queue) {
  Trade trade;
  while (keep_running.load()) {
    if (trades_queue.try_pop(trade)) {
      std::cout << "Trade price: " << trade.price << std::endl;
      std::cout << "Trade quantity: " << trade.quantity << std::endl;
      std::cout << "Trade Maker: " << trade.maker_order_id << std::endl;
      std::cout << "Trade Taker: " << trade.taker_order_id << std::endl;
      std::cout << "Trade Time: " << trade.time_stamp << std::endl;
      // std::cout << "Trade Aggressor: " << trade.aggressor_side << std::endl;
      std::cout << "---" << std::endl;
    }
  }
}

void signal_handler(int /*unused*/) {
  keep_running.store(false);
  std::cout << "Exchange has closed\n";
}

auto main() -> int {
  // The main entry point of our simulation.

  // Signal handler for Ctrl + C
  std::signal(SIGINT, signal_handler);

  // Initilialing classed and data structures.
  threadsafe::stl_queue<ClientRequest> event_queue;
  threadsafe::stl_queue<Trade> trades_queue;
  threadsafe::stl_queue<ExecutionReport> execution_report;
  OrderBook order_book;

  GateWay gateway(event_queue, execution_report);
  Engine engine(event_queue, trades_queue, execution_report, order_book);

  int num_client_threads = 4;
  std::vector<Client> clients;
  std::vector<std::thread> client_orders;
  clients.reserve(num_client_threads);
  client_orders.reserve(num_client_threads);
  for (int i = 1; i <= num_client_threads; i++) {
    clients.emplace_back(i, gateway, trades_queue);
    client_orders.emplace_back(&Client::run, &clients.back());
    gateway.addClient(i, &clients.back());
  }
  // Let's just add two clients for now.
  // NOTE: currently facilities for starting and stopping are not given.
  // Currently relying on process termination to kill all threads.

  std::thread engine_event_handler(&Engine::handleEvents, &engine);
  std::thread execution_report_dispatcher(&GateWay::dispatcher, &gateway);
  // Exchange started!
  start_exchange.store(true, std::memory_order_release);
  std::cout << "Exchange has opened\n";
  auto start = std::chrono::steady_clock::now();

  // For rough debugging only!
  // std::thread print_trades(printTrades, std::ref(trades_queue));

  for (auto& client_order : client_orders) {
    client_order.join();  // Joining the clients.
  }
  engine_event_handler.join();
  execution_report_dispatcher.join();
  // print_trades.join();
  auto end = std::chrono::steady_clock::now();
  std::cout << "Exchange ran for "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << "ms\n";

  std::ofstream file;
  // Writing trades.
  file.open("logs/processed_trades.txt", std::ios::out);
  file << "Processed Trades\n";
  file << trades_queue.size() << " Trades Processed\n";
  Trade trade;
  while (trades_queue.try_pop(trade)) {
    file << "MAKER: " << trade.maker_order_id
         << " TAKER: " << trade.taker_order_id << " " << trade.quantity << " @ "
         << trade.price << " TIMESTAMP-" << trade.time_stamp << "\n";
  }
  file.close();
  // Writing all execution reports.
}
