#include "engine/gateway.hpp"

#include <chrono>

#include "containers/lock_queue.hpp"
#include "engine/client.hpp"
#include "engine/types.hpp"
GateWay::GateWay(threadsafe::stl_queue<ClientRequest>& event_queue,
                 threadsafe::stl_queue<ExecutionReport>& exec_report)
    : event_queue(event_queue), execution_reports(exec_report) {}

void GateWay::addOrder(const Order& order, ClientId cid) {
  ClientRequest client_request;
  client_request.client_id = cid;
  client_request.time_stamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  client_request.type = RequestType::New;
  client_request.new_order = order;
  event_queue.push(client_request);
}

void GateWay::cancelOrder(OrderId order_id, ClientId cid) {
  ClientRequest client_request;
  client_request.client_id = cid;
  client_request.time_stamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  client_request.order_id_to_cancel = order_id;
  client_request.type = RequestType::Cancel;
  event_queue.push(client_request);
}

void GateWay::dispatcher() {}
