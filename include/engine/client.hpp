#ifndef CLIENT_HPP
#define CLIENT_HPP

// #define DEBUG
#include <random>
#include <vector>

#include "containers/lock_queue.hpp"
#include "gateway.hpp"
#include "types.hpp"
// This class serves as a sample class for a client.
class GateWay;  // NOTE: Do not forget to include gatway in cpp file.
class Client {
 private:
  ClientId my_id;
#ifndef DEBUG
  static std::random_device rand_device;
  static std::mt19937 gen;
  static std::uniform_int_distribution<int> distrib;
  static std::uniform_int_distribution<int> distrib_bool;
#endif
  GateWay& gateway;
  threadsafe::stl_queue<Trade>&
      trades_queue;  // Contains actual data regarding trades.
  threadsafe::stl_queue<ExecutionReport> reports;  // My reports.
  std::vector<Trade>
      trades;  // Can be used for storing trades, may be needed by client.

  void placeOrder(const Order& order);  // Runs as background thread and Calls
                                        // gateway.placeOrder()
  static auto generateOrder()
      -> Order;  // Generates order based on a strategy. TBD later.
 public:
  Client(ClientId my_id, GateWay& gtway,
         threadsafe::stl_queue<Trade>& trades_queue);  // Constructor
  void readTrades();   // Runs as another thread and continuosly reads trades.
  void readReports();  // Runs as another thread and reads Execution reports and
                       // trades for better strategy.
  void generateAndPlaceOrders();  // Runs continuosly in a loop, calls
                                  // generateOrder and placeOrder
};

#endif
