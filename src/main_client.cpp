#include <thread>

#include "network/client.hpp"

auto main() -> int {
  Client client;
  client.init("localhost", "12345");
  std::thread exchange_data(&Client::moveData, &client);
  std::thread generate_orders(&Client::generateOrders, &client);
  exchange_data.join();  // Note that these will never join rght now.
  generate_orders.join();
}
