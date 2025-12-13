#include "engine/client.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <random>

#include "containers/lock_queue.hpp"
#include "engine/constants.hpp"
#include "engine/gateway.hpp"
#include "engine/types.hpp"
// NOTE: currently pricing true cost at 100. People randomly select any number
// between 105 and 95.
extern std::atomic<bool> keep_running;
extern std::atomic<bool> start_exchange;

Client::Client(ClientId my_id, GateWay& gtway,
               threadsafe::stl_queue<Trade>& trades_queue)
    : my_id(my_id),
      gateway(gtway),
      trades_queue(trades_queue),
      gen(std::random_device{}()),
      distrib(CLIENT_PRICE_DISTRIB_MIN, CLIENT_PRICE_DISTRIB_MAX),
      distrib_bool(0, 1) {
  this->local_order_id = 1;  // Otherwise cancellaiton one can give problems.
  std::ofstream file;
  std::string filename =
      std::format("logs/execution_reports_client_{}.txt", my_id);
  file.open(filename, std::ios::out);
  file << "Execution Reports for Client " << my_id << "\n";
}

Client::~Client() { writeExecutionReport(); }

// I understand this is a very bad method for order generation. But I just want
// to test now.
// TODO: add cancellation facility.
auto Client::generateOrder() -> Order {
  Order order;
  order.order_id =
      (static_cast<uint64_t>(my_id) << LOCAL_ORDER_BITS | local_order_id++);
  order.price = (CLIENT_BASE_PRICE) + distrib(gen);
  order.quantity = CLIENT_BASE_QUANTITY + distrib(gen);
  int r_int_side = distrib_bool(gen);
  int r_int_order_type = distrib_bool(gen);
  int r_int_tif = distrib_bool(gen);
  order.side = static_cast<Side>(r_int_side);
  order.order_type = static_cast<OrderType>(r_int_order_type);
  order.tif = static_cast<TimeInForce>(r_int_tif);

  return order;
}

void Client::run() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    // TODO: learn what memory_order_acquire actually i
    std::this_thread::yield();
  }
  while (keep_running.load(std::memory_order_relaxed)) {
    size_t count = 0;
    for (int i = 0; i < 7500; i++) {
      count += distrib(gen) % 10000;
    }  // Sleep for isn't accurate enough.
    // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
    gateway.addOrder(generateOrder(), my_id);
    if (local_order_id % (ORDER_CANCELLATION_FREQ) == 0) {
      OrderId to_delete = local_order_id - (rand() % (ORDER_CANCELLATION_FREQ));
      gateway.cancelOrder((my_id << (LOCAL_ORDER_BITS) | to_delete), my_id);
    }
    if (reports.size() >= MAX_EXECUTION_REPORTS_SIZE) {
      writeExecutionReport();  // Clients duty to write execution reports.
    }
  }
}

void Client::readTrades() {
  while (true) {
  }
}

void Client::addReport(ExecutionReport exec_report) {
  std::lock_guard<std::mutex> lock(report_mutex);
  reports.push_back(exec_report);
}

void Client::writeExecutionReport() {
  std::ofstream file;
  std::string filename =
      std::format("logs/execution_reports_client_{}.txt", my_id);
  file.open(filename, std::ios::app);
  std::lock_guard<std::mutex> lock(report_mutex);
  for (auto report : reports) {
    file << "CLIENT " << report.client_id << " "
         << "ORDER ID " << report.order_id << " "
         << "PRICE " << report.price << " "
         << "LAST QUANTITY " << report.last_quantity << " "
         << "REMAINING QUANTITY " << report.remaining_quantity << " "
         << (report.side == Side::BID ? "BUY " : "SELL ") << "EXEC TYPE ";
    switch (report.type) {
      case ExecType::NEW:
        file << "NEW ";
        break;
      case ExecType::CANCELED:
        file << "CANCELED ";
        break;
      case ExecType::REJECTED:
        file << "REJECTED - ";
        switch (report.reason) {
          case RejectReason::NONE:
            file << "NONE ";
            break;
          case RejectReason::ORDER_NOT_FOUND:
            file << "ORDER_NOT_FOUND ";
            break;
          case RejectReason::PRICE_INVALID:
            file << "PRICE_INVALID ";
            break;
          case RejectReason::QUANTITY_INVALID:
            file << "QUANTITY_INVALID ";
            break;
          case RejectReason::MARKET_CLOSED:
            file << "MARKET_CLOSED ";
            break;
          case RejectReason::SELF_TRADE:
            file << "SELF_TRADE ";
            break;
          case RejectReason::INVALID_ORDER_TYPE:
            file << "INVALID_ORDER_TYPE ";
            break;
        }
        break;
      case ExecType::TRADE:
        file << "TRADE ";
        break;
      case ExecType::EXPIRED:
        file << "EXPIRED ";
        break;
    }
    file << "\n";
  }
  file.close();
  reports.clear();
}
