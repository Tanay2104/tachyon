#include "network/client.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <endian.h>
#include <err.h>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "engine/concepts.hpp"
#include "engine/constants.hpp"
#include "engine/types.hpp"
#include "my_config.hpp"
#include "network/serialise.hpp"

// NOTE: the report writing scheme isn't good rigth now.
// Especially because ID's are not known at initilasation.
template <TachyonConfig config>
Client<config>::Client()
    : generator(std::random_device{}()), distribution(CLIENT_BASE_PRICE, 500),
      rx_buffer(1024), tx_buffer(1024) {

  std::ofstream file;
  std::string filename =
      std::format("logs/execution_reports_client_{}.txt", my_id);
  file.open(filename, std::ios::out);
  file << "Execution Reports for Client " << my_id << "\n";
}

template <TachyonConfig config> Client<config>::~Client() { writeReports(); }

template <TachyonConfig config>
void Client<config>::init(std::string host, std::string port) {
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct addrinfo *ptr;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int return_value = getaddrinfo(host.data(), port.data(), &hints, &servinfo);
  if (return_value != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(return_value));
    throw std::runtime_error("Error getting address info");
  }
  // loop through results and bind to firs active one.
  for (ptr = servinfo; ptr != nullptr; ptr = ptr->ai_next) {
    sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sockfd == -1) {
      perror("server: socket");
      continue;
    }
    if (connect(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
      perror("client: connect");
      continue;
    }
    break; // we potentially found our socket.
  }

  if (ptr == nullptr) {
    fprintf(stderr, "client: failed to connect\n");
    throw std::runtime_error("Could not connect");
  }

  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
  std::cout << "Client connected to server\n";
  freeaddrinfo(servinfo);
}

template <TachyonConfig config> auto Client<config>::flushBuffer() -> bool {
  if (tx_buffer.size() == 0) {
    return true;
  }
  const uint8_t *data_ptr = tx_buffer.begin();
  size_t remaining = tx_buffer.size();

  // non blocking send.
  ssize_t sent = send(sockfd, data_ptr, remaining, MSG_DONTWAIT);

  if (sent > 0) {
    tx_buffer.erase(sent);
  } else if (sent == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // kernel buffer full. stop trying.
      return false;
    }
    // some other iisue. fix later.
    return false;
  }

  if (tx_buffer.size() == 0) {
    tx_buffer.clear();
    return true;
  }
  return false;
}

template <TachyonConfig config>
void Client<config>::updateEpoll(int epoll_fd, bool listen_for_write) {
  struct epoll_event evt;
  evt.data.fd = sockfd;
  evt.events = EPOLLIN;
  if (listen_for_write) {
    evt.events |= EPOLLOUT;
  }
  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &evt);
}

template <TachyonConfig config> void Client<config>::moveData() {
  // recv -> rx_buffer -> deserialise -> exec reports queue -> used by
  // strategist. tx_buffer -> flush.
  // TODO: make some proper mechanism to close true loop

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }
  struct epoll_event evt;
  struct epoll_event events;
  evt.events = EPOLLIN;
  evt.data.fd = sockfd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &evt) == -1) {
    throw std::runtime_error("epoll_ctl: sockfd");
  }
  std::cout << "Client loop started\n";
  while (true) {
    int n_ready_fd = epoll_wait(epoll_fd, &events, 1, 1);
    if (n_ready_fd == -1) {
      perror("epoll_wait");
      break;
    }
    // Now we handle transmit.
    uint8_t serialise_buf[64];
    int pops = 0;
    Order order{};
    while (pops < 100 && orders_to_place.try_pop(order)) {
      size_t len = serialise_order(order, serialise_buf);
      tx_buffer.insert(serialise_buf, len);
      pops++;
    }
    pops = 0;
    OrderId to_cancel;
    while (pops < 100 && cancels_to_place.try_pop(to_cancel)) {
      size_t len = serialise_order_cancel(to_cancel, serialise_buf);
      tx_buffer.insert(serialise_buf, len);
      pops++;
    }

    bool all_sent = flushBuffer();
    // upon printing all sent is almost always true.
    // std::cout << "all sent = " << all_sent << "\n";
    if (!all_sent) {
      // we have data stuck in tx buffer.
      // So we need to listen for when we can write again.
      updateEpoll(epoll_fd, true);
    } else {
      updateEpoll(epoll_fd, false);
    }

    int timeout = (orders_to_place.empty() && cancels_to_place.empty()) ? 1 : 0;

    n_ready_fd = epoll_wait(epoll_fd, &events, 1, timeout);
    if (n_ready_fd > 0) {
      if (bool(events.events & EPOLLIN)) { // it is a read event.
        uint8_t temp_buff[MAX_TEMP_BUF_SIZE];
        ssize_t bytes_read =
            recv(events.data.fd, temp_buff, sizeof(temp_buff), 0);

        if (bytes_read <= 0) {
          if (bytes_read < 0 && errno == EAGAIN) {
            // spurious wakeup?
            //
          } else {
            std::cout << "Server disconnected.\n";
            close(events.data.fd);
            return;
          }
        }
        rx_buffer.insert(temp_buff, bytes_read);
        // drain as many full messages as possible.
        drainRx();

        // Read Write cycle complete!!!
      }
      if (bool(events.events & EPOLLOUT)) {
        // we can write again now.
        flushBuffer();
      }
    }
  }
}

template <TachyonConfig config> void Client<config>::drainRx() {
  while (rx_buffer.size() > 0) {
    uint8_t msg_type = *rx_buffer.begin();
    uint32_t expected_len = 0;

    if (msg_type == static_cast<uint8_t>(MessageType::LOGIN_RESPONSE)) {
      expected_len = 5; //  1 + 4 for client.
    }
    // TODO: there should be a feature for occur once only events right? Look
    // that up?
    else if (msg_type == static_cast<uint8_t>(MessageType::EXEC_REPORT)) {
      expected_len = 1 + sizeof(ExecutionReport);
    } else {
      // invalid data.
      // Should we do anything.
      // i guess not.
      break;
    }
    if (rx_buffer.size() < expected_len) {
      break;
    }
    // full message available!.
    uint8_t *msg_start = rx_buffer.begin();
    if (msg_type == static_cast<uint8_t>(MessageType::LOGIN_RESPONSE)) {
      uint32_t net_id;
      std::memcpy(&net_id, &msg_start[1], 4);
      my_id = be32toh(net_id);
    }

    else if (msg_type == static_cast<uint8_t>(MessageType::EXEC_REPORT)) {
      ExecutionReport exec_report;
      deserialise_execution_report(msg_start, exec_report);
      reports.push(exec_report); // thread safe queue.
    }
    // erase old data.
    rx_buffer.erase(expected_len);
  }
}

template <TachyonConfig config> void Client<config>::generateOrders() {
  // TODO: add some better methods to break the loop maybe?

  // NOTE: we are currenlty not using any market data or strategy.
  // These are to be added later.
  while (true) {
    if (my_id == std::numeric_limits<ClientId>::max()) {
      std::this_thread::yield();
      std::cout << "meow didn't recieve id\n";
      // we haven't recieved id yet.
    }
    // std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    orders_to_place.push(generateOrderHelper());
    if (local_order_id % (ORDER_CANCELLATION_FREQ) == 0) {
      OrderId to_delete = local_order_id - (rand() % (ORDER_CANCELLATION_FREQ));
      cancels_to_place.push(
          (static_cast<uint64_t>(my_id) << (LOCAL_ORDER_BITS) | to_delete));
    }
  }
}

template <TachyonConfig config>
auto Client<config>::generateOrderHelper() -> Order {
  Order order;
  order.order_id =
      (static_cast<uint64_t>(my_id) << LOCAL_ORDER_BITS | local_order_id++);
  // We use random walk.
  // clients are fast enough so rand doesn't matter much.
  double result = distribution(generator);
  while (result <= 0) {
    result = distribution(generator);
  }
  order.price = Price(result);
  order.price = std::max<Price>(
      order.price, Price(CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN));

  order.price = std::min<Price>(
      order.price, Price(CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MAX));
  order.quantity = CLIENT_BASE_QUANTITY + (rand() % 1000);
  order.side = static_cast<Side>(rand() % 2);
  order.order_type = OrderType::LIMIT;
  order.tif = TimeInForce::GTC;

  // this cout is outputting correctly.
  /* std::cout << "I placed new Order with Order id = " << order.order_id
            << " client id: " << my_id << " price : " << order.price
            << " quantity:  " << order.quantity << "\n"; */
  return order;
}

// Clients are responsible for their own execution reports.
template <TachyonConfig config> void Client<config>::writeReportsContinuous() {
  while (true) {
    if (reports.size() % MAX_EXECUTION_REPORTS_SIZE == 0) {
      writeReports();
    }
  }
}

template <TachyonConfig config> void Client<config>::writeReports() {
  std::ofstream file;
  std::string filename =
      std::format("logs/execution_reports_client_{}.txt", my_id);
  file.open(filename, std::ios::app);
  ExecutionReport report;
  while (reports.try_pop(report)) {
    file << "CLIENT " << report.client_id << " "
         << "ORDER ID " << report.order_id << " "
         << "PRICE " << report.price << " "
         << "LAST QUANTITY " << report.last_quantity << " "
         << "REMAINING QUANTITY " << report.remaining_quantity << " "
         << (report.side == Side::BID ? "BUY " : "SELL ") << "EXEC TYPE ";
    switch (report.type) {
    case ExecType::NEW:
      file << "NEW ";
      break;
    case ExecType::CANCELED:
      file << "CANCELED ";
      break;
    case ExecType::REJECTED:
      file << "REJECTED - ";
      switch (report.reason) {
      case RejectReason::NONE:
        file << "NONE ";
        break;
      case RejectReason::ORDER_NOT_FOUND:
        file << "ORDER_NOT_FOUND ";
        break;
      case RejectReason::PRICE_INVALID:
        file << "PRICE_INVALID ";
        break;
      case RejectReason::QUANTITY_INVALID:
        file << "QUANTITY_INVALID ";
        break;
      case RejectReason::MARKET_CLOSED:
        file << "MARKET_CLOSED ";
        break;
      case RejectReason::SELF_TRADE:
        file << "SELF_TRADE ";
        break;
      case RejectReason::INVALID_ORDER_TYPE:
        file << "INVALID_ORDER_TYPE ";
        break;
      }
      break;
    case ExecType::TRADE:
      file << "TRADE ";
      break;
    case ExecType::EXPIRED:
      file << "EXPIRED ";
      break;
    }
    file << "\n";
  }
  file.close();
}
template class Client<my_config>;
