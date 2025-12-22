#include "engine/concepts.hpp"
#include "my_config.hpp"
#include "network/client.hpp"
#include <thread>

auto main() -> int {
  int num_clients = 4;
  std::vector<Client<my_config> *> clients;
  std::vector<std::thread> exchange_data;
  std::vector<std::thread> generate_orders;
  std::vector<std::thread> write_reports;
  for (int i = 0; i < num_clients; i++) {
    auto *new_client = new Client<my_config>();
    clients.push_back(new_client);
    new_client->init("localhost", "12345");
    exchange_data.emplace_back(&Client<my_config>::moveData, new_client);
    generate_orders.emplace_back(&Client<my_config>::generateOrders,
                                 new_client);
    write_reports.emplace_back(&Client<my_config>::writeReportsContinuous,
                               new_client);
  }

  for (int i = 0; i < num_clients; i++) {
    exchange_data[i].join();
    generate_orders[i].join(); // Note that these will never join rght now.
    delete clients[i];
  }
  // Let there be memory leak. We'll find better harnesses later.
}
