#include "engine/client.hpp"

#include <cstdint>
#include <random>

#include "containers/lock_queue.hpp"
#include "engine/gateway.hpp"
#include "engine/types.hpp"
// NOTE: currently pricing true cost at 100. People randomly select any number
// between 105 and 95.
extern std::atomic<bool> keep_running;
extern std::atomic<bool> start_exchange;
std::random_device Client::rand_device;
std::mt19937 Client::gen(Client::rand_device());
std::uniform_int_distribution<int> Client::distrib(-5000, 5000);
std::uniform_int_distribution<int> Client::distrib_bool(0, 1);
Client::Client(ClientId my_id, GateWay& gtway,
               threadsafe::stl_queue<Trade>& trades_queue)
    : my_id(my_id), gateway(gtway), trades_queue(trades_queue) {
  this->local_order_id = 1;  // Otherwise cancellaiton one can give problems.
}

// I understand this is a very bad method for order generation. But I just want
// to test now.
// TODO: add cancellation facility.
auto Client::generateOrder() -> Order {
  Order order;
  order.order_id = (my_id << 16 | local_order_id++);
  order.price = (100 * 1000) + distrib(gen);
  order.quantity = 50000 + distrib(gen);
  int r_int_side = distrib_bool(gen);
  // int r_int_order_type = distrib_bool(gen);
  // int r_int_tif = distrib_bool(gen);
  order.side = static_cast<Side>(r_int_side);
  order.order_type = OrderType::LIMIT;
  order.tif = TimeInForce::GTC;

  return order;
}

void Client::run() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    // TODO: learn what memory_order_acquire actually i
    std::this_thread::yield();
  }
  while (keep_running.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    gateway.addOrder(generateOrder(), my_id);
    if (local_order_id % 50 == 0) {
      OrderId to_delete = local_order_id - (rand() % 50);
      gateway.cancelOrder((my_id << 16 | to_delete), my_id);
    }
  }
}

void Client::readTrades() {
  while (true) {
  }
}

void Client::addReport(ExecutionReport exec_report) {
  reports.push_back(exec_report);
}
