#pragma once
#include <limits>
#include <random>
#include <vector>

#include "containers/lock_queue.hpp"
#include "engine/concepts.hpp"
#include "engine/constants.hpp"
#include "engine/types.hpp"

// TODO: eventually make a client config.
template <TachyonConfig config> class Client {
private:
  int sockfd;
  ClientId my_id = std::numeric_limits<ClientId>::max();
  OrderId local_order_id{};

  std::mt19937 generator;
  std::normal_distribution<double> distribution;

  static constexpr int MAX_TEMP_BUF_SIZE = 1024;

  config::RxBufferType rx_buffer; // Only accessed by one thread.
  config::TxBufferType tx_buffer;
  config::ExecReportQueue
      reports; // can be used by strategy thread for better strategies.
  size_t tx_offset{};
  threadsafe::stl_queue<Order> orders_to_place; // generateOrders decides.
  threadsafe::stl_queue<OrderId> cancels_to_place;
  // Helper functions.
  auto flushBuffer() -> bool;
  void drainRx();
  void updateEpoll(int epoll_fd, bool listen_for_write);

  auto generateOrderHelper() -> Order;

public:
  Client();
  void init(std::string host, std::string port);
  void moveData();       // function which sends and recieves data.
  void generateOrders(); // generates requests using strategies.
};
