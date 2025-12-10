#ifndef GATEWAY_HPP
#define GATEWAY_HPP

#include <unordered_map>

#include "../containers/lock_queue.hpp"
#include "types.hpp"

class Client;  // NOTE: do not forget to include client in cpp file.
class GateWay {
 private:
  std::unordered_map<ClientId, Client*> a;
  threadsafe::stl_queue<Order*>& event_queue;

 public:
  GateWay(threadsafe::stl_queue<Order*>& event_queue);
};

#endif
