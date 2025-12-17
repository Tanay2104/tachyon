#pragma once
#include <random>
#include <vector>

#include "containers/lock_queue.hpp"
#include "engine/constants.hpp"
#include "engine/types.hpp"

class Client {
 private:
  int sockfd;
  ClientId my_id;
  OrderId local_order_id{};

  std::mt19937 generator;
  std::normal_distribution<double> distribution;

  static constexpr int MAX_TEMP_BUF_SIZE = 1024;

  std::vector<uint8_t> rx_buffer;  // Only accessed by one thread.
  threadsafe::stl_queue<ExecutionReport>
      reports;  // can be used by strategy thread for better strategies.
  std::vector<uint8_t> tx_buffer;  // Only accessed by one thread.
  size_t tx_offset;
  threadsafe::stl_queue<Order> orders_to_place;  // generateOrders decides.
  threadsafe::stl_queue<OrderId> cancels_to_place;
  // Helper functions.
  auto flushBuffer() -> bool;
  void drainRx();
  void updateEpoll(int epoll_fd, bool listen_for_write);

  auto generateOrderHelper() -> Order;

 public:
  Client();
  void init(std::string host, std::string port);
  void moveData();        // function which sends and recieves data.
  void generateOrders();  // generates requests using strategies.
};
