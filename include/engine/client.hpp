#ifndef CLIENT_HPP
#define CLIENT_HPP

// #define DEBUG
#include <mutex>
#include <random>
#include <unordered_set>
#include <vector>

#include "containers/lock_queue.hpp"
#include "gateway.hpp"
#include "types.hpp"
// This class serves as a sample class for a client.
class GateWay;  // NOTE: Do not forget to include gatway in cpp file.
class Client {
 private:
  ClientId my_id;
  OrderId local_order_id;

  GateWay& gateway;
  threadsafe::stl_queue<Trade>&
      trades_queue;  // Contains actual data regarding trades.
  std::vector<ExecutionReport> reports;  // My reports.
  std::mutex report_mutex;
  std::vector<Trade>
      trades;  // Can be used for storing trades, may be needed by client.
  std::random_device rand_device;
  std::mt19937 gen;
  std::uniform_int_distribution<int> distrib;
  std::uniform_int_distribution<int> distrib_bool;

  void readTrades();  // Can be used by trader to improve strategy.
  void placeOrder(const Order& order);  // Runs as background thread and Calls
                                        // gateway.placeOrder()
  auto generateOrder()
      -> Order;  // Generates order based on a strategy. TBD later.
  void writeExecutionReport();

 public:
  Client(ClientId my_id, GateWay& gtway,
         threadsafe::stl_queue<Trade>& trades_queue);  // Constructor
  ~Client();
  void run();  // Runs continuosly in a loop, calls
               // generateOrder and placeOrder
  void addReport(
      ExecutionReport exec_report);  // Called by dispatcher for adding reports
                                     // to personal queue.
};

#endif
