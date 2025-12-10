#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../containers/lock_queue.hpp"
#include "gateway.hpp"
#include "types.hpp"
// This class serves as a sample class for a client.
class GateWay;  // NOTE: Do not forget to include gatway in cpp file.
class Client {
 private:
  ClientId my_id;
  GateWay& gateway;
  threadsafe::stl_queue<ExecutionReport> reports;

 public:
  Client(GateWay& gtway);
};

#endif
