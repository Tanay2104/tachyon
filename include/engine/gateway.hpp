#ifndef GATEWAY_HPP
#define GATEWAY_HPP

#include <unordered_map>

#include "../containers/lock_queue.hpp"
#include "types.hpp"

class Client;  // NOTE: do not forget to include client in cpp file.
class GateWay {
 private:
  std::unordered_map<ClientId, Client*> a;
  threadsafe::stl_queue<ClientRequest>& event_queue;
  threadsafe::stl_queue<ExecutionReport>& execution_reports;

 public:
  GateWay(threadsafe::stl_queue<ClientRequest>& event_queue,
          threadsafe::stl_queue<ExecutionReport>& exec_reports);
  void addOrder(const Order& order, ClientId cid);
  void cancelOrder(OrderId order_id, ClientId client_id);
  void dispatcher();  // Sends the execution reports to the individual clients.
};

#endif
