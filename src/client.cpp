#include "engine/client.hpp"

#include <iostream>
#include <random>
extern OrderId order_id;
#include "containers/lock_queue.hpp"
#include "engine/gateway.hpp"
#include "engine/types.hpp"
// NOTE: currently pricing true cost at 100. People randomly select any number
// between 105 and 95.
#ifndef DEBUG
std::random_device Client::rand_device;
std::mt19937 Client::gen(Client::rand_device());
std::uniform_int_distribution<int> Client::distrib(-5000, 5000);
std::uniform_int_distribution<int> Client::distrib_bool(0, 2);
#endif
Client::Client(ClientId my_id, GateWay& gtway,
               threadsafe::stl_queue<Trade>& trades_queue)
    : my_id(my_id), gateway(gtway), trades_queue(trades_queue) {}

// I understand this is a very bad method for order generation. But I just want
// to test now.
// TODO: add cancellation facility.
#ifndef DEBUG
auto Client::generateOrder() -> Order {
  Order order;
  order.order_id = order_id++;
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
#endif

#ifdef DEBUG
auto Client::generateOrder() -> Order {
  std::cout
      << "DEBUG mode for generating order\n Enter order id price quantity "
         "side(0 ask 1 bid)\n Order type will be limit and tif will be GTC";
  Order order;
  bool side;
  std::cin >> order.order_id >> order.price >> order.quantity >> side;
  order.side = static_cast<Side>(side);
  std::cout << "order id = " << order.order_id << std::endl;
  return order;
}
#endif  // !DEBUG

void Client::generateAndPlaceOrders() {
  while (true) {
    gateway.addOrder(generateOrder(), my_id);
  }
}

void Client::readTrades() {
  while (true) {
  }
}

void Client::readReports() {
  while (true) {
  }
}
