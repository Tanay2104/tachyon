#include <thread>

#include "containers/lock_queue.hpp"
#include "engine/client.hpp"
#include "engine/engine.hpp"
#include "engine/gateway.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"

OrderId order_id;
auto main() -> int {
  threadsafe::stl_queue<ClientRequest> event_queue;
  threadsafe::stl_queue<Trade> trades_queue;
  OrderBook order_book;

  GateWay gateway(event_queue);
  Engine engine(event_queue, order_book, trades_queue);

  Client foo(0, gateway, trades_queue);
  // Client bar(1, gateway, trades_queue);
  // Let's just add two clients for now.
  // NOTE: currently facilities for starting and stopping are not given.
  // Currently relying on process termination to kill all threads.

  // TODO: add more and dynamic clients.

  std::thread foo_place_order(&Client::generateAndPlaceOrders, &foo);
  // std::thread bar_place_order(&Client::generateAndPlaceOrders, &bar);

  std::thread engine_event_handler(&Engine::handleEvents, engine);
  // std::thread foo_read_reports(&Client::readReports, &foo);  // Currently just empty.
  // std::thread bar_read_reports(&Client::readReports, &bar);  // Currently
  // just empty.

  // std::thread foo_read_trades(&Client::readReports, &foo);
  // std::thread bar_read_trades(&Client::readReports, &bar);
  // Lots of threads!
  foo_place_order.join();
  engine_event_handler.join();
}
