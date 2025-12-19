#include "engine/concepts.hpp"
#include "my_config.hpp"
#include "network/client.hpp"
#include <thread>

auto main() -> int {
  Client<my_config> client;
  client.init("localhost", "12345");
  std::thread exchange_data(&Client<my_config>::moveData, &client);
  std::thread generate_orders(&Client<my_config>::generateOrders, &client);
  exchange_data.join(); // Note that these will never join rght now.
  generate_orders.join();
}
