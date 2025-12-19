#include "network/tcpserver.hpp"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "engine/concepts.hpp"
#include "engine/types.hpp"
#include "my_config.hpp"
#include "network/serialise.hpp"

extern std ::atomic<bool> keep_running;

template <TachyonConfig config>
void TcpServer<config>::setNonBlocking(int file_descriptor) {
  int flags = fcntl(file_descriptor, F_GETFL, 0);
  fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK);
}

template <TachyonConfig config>
TcpServer<config>::TcpServer(config::EventQueue &event_queue,
                             config::ExecReportQueue &execution_reports)
    : event_queue(event_queue), execution_reports(execution_reports),
      client_map(16) {
  next_id.store(1);
}

template <TachyonConfig config>
void TcpServer<config>::init(
    std::string port) { // small string so no need to pass by reference.
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct addrinfo *ptr;
  int yes = 1;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // auto detect IP for me.

  int return_value = getaddrinfo(NULL, port.data(), &hints, &servinfo);
  if (return_value != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(return_value));
    throw std::runtime_error("Error getting address info");
  }
  // loop through results and bind to firs active one.
  for (ptr = servinfo; ptr != nullptr; ptr = ptr->ai_next) {
    listen_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (listen_fd == -1) {
      perror("server: socket");
      continue;
    }
    int sockopt_result =
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (sockopt_result == -1) {
      perror("setsockopt");
      continue;
    }
    int bind_result = bind(listen_fd, ptr->ai_addr, ptr->ai_addrlen);
    if (bind_result == -1) {
      close(listen_fd);
      perror("server: bind");
      continue;
    }
    break; // we potentially found our socket.
  }

  freeaddrinfo(servinfo);

  if (ptr == nullptr) {
    fprintf(stderr, "server: failed to bind\n");
    throw std::runtime_error("Server failed to bind");
  }
  int listen_result = listen(listen_fd, BACKLOG);
  if (listen_result == -1) {
    perror("listen");
    throw std::runtime_error("Error trying to listen");
  }
  // success!
  std::cout << "Server initialised. Waiting for connections.\n";
}

// designed to run on a single thread to accept orders.

template <TachyonConfig config> void TcpServer<config>::receiveData() {
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }
  struct epoll_event evt;
  struct epoll_event events[MAX_EPOLL_EVENTS];
  evt.events = EPOLLIN;    // only reading for new order, cancel order.
  evt.data.fd = listen_fd; // listening for new clients.

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &evt) == -1) {
    throw std::runtime_error("epoll_ctl: listen_fd");
  }
  std::cout << "Engine event loop started\n";

  while (keep_running.load()) {

    int n_ready_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS,
                                 -1); // Partially blocking call..

    if (n_ready_fds == -1) {
      perror("epoll_wait");
      break;
    }
    for (int i = 0; i < n_ready_fds; i++) {
      if (events[i].data.fd == listen_fd) {
        handleNewConnection(evt, epoll_fd);
      } else {
        // data from existing client.
        auto *conn =
            (ClientConnection<typename config::RxBufferType,
                              typename config::TxBufferType> *)events[i]
                .data.ptr;
        uint8_t temp_buff[MAX_TEMP_BUFF_SIZE];
        ssize_t bytes_read = recv(conn->fd, temp_buff, sizeof(temp_buff), 0);

        if (bytes_read <= 0) {
          // handle disconnect or error.
          if (bytes_read < 0 && errno == EAGAIN) {
            continue; // spurious wakeup.
          }
          std::cout << "Client disconnected\n";
          close(conn->fd);
          // TODO: there must be a faster way for client map.
          if (client_map.contains(conn->client_id)) {
            client_map.erase(conn->client_id);
          }
          // NOTE: may release in heap use after free if deleted conn
          // prematurely. So commented out for now. Memory leak will be there.
          // delete conn;
          continue;
        }
        conn->rx_buffer.insert(temp_buff, bytes_read);

        // drain as many full messages as possible.
        while (conn->rx_buffer.size() > 0) {
          uint8_t msg_type = *conn->rx_buffer.begin();
          uint32_t expected_len = 0;
          uint8_t *raw = conn->rx_buffer.begin();
          if (msg_type == static_cast<uint8_t>(MessageType::ORDER_NEW)) {
            expected_len = 1 + sizeof(Order);
          }

          else if (msg_type ==
                   static_cast<uint8_t>(MessageType::ORDER_CANCEL)) {
            expected_len = 9; // Update this if cancel struct changes.
          } else {
            // Invalid data.
            std::cout << "Bad client invalid data, closing\n";
            std::cout << "Client requested msg type = "
                      << static_cast<int>(msg_type) << "\n";
            close(conn->fd);

            if (client_map.contains(conn->client_id)) {
              std::cout << "Client was there in hash map\n";
              client_map.erase(conn->client_id);
            }
            // Bad client.
            delete conn;
            break;
          }

          if (conn->rx_buffer.size() < expected_len) {
            // Not enough data yet. Wait for next call.
            break;
          }
          // We have a full message!
          uint8_t *msg_start = conn->rx_buffer.begin();
          if (msg_type == static_cast<uint8_t>(MessageType::ORDER_NEW)) {
            handleNewOrder(msg_start, conn->client_id);
          }

          else if (msg_type ==
                   static_cast<uint8_t>(MessageType::ORDER_CANCEL)) {
            handleCancellation(msg_start, conn->client_id);
          }
          // nothing else should happen.

          // erase old data.
          conn->rx_buffer.erase(expected_len);
        }
      }
    }
  }
}

template <TachyonConfig config>
void TcpServer<config>::handleNewConnection(struct epoll_event evt,
                                            int epoll_fd) {
  // New connection.
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof addr;
  int client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_len);
  if (client_fd == -1) {
    perror("accept");
    return;
  }
  setNonBlocking(client_fd);

  auto *conn = new ClientConnection<typename config::RxBufferType,
                                    typename config::TxBufferType>(client_fd);
  conn->client_id = next_id.fetch_add(1);
  evt.events = EPOLLIN;
  evt.data.ptr = conn;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &evt) == -1) {
    perror("epoll_ctl: connected socket");
    delete conn; // delete if connection failed.
    std::cout << "client is deleted wrong socket\n";
    close(client_fd);
  }

  client_map.insert({conn->client_id, conn});

  uint8_t welcome[5];
  serialise_new_login(conn->client_id, welcome);
  // TODO: this is a blocking call. ideally use tx buffer.
  send(client_fd, &welcome, sizeof(welcome), 0);
  std::cout << "New client connected: fd = " << client_fd
            << " with assigned id = " << conn->client_id << "\n";
}

template <TachyonConfig config>
void TcpServer<config>::handleNewOrder(uint8_t *buffer, ClientId cid) {
  Order order;
  ClientRequest clr;
  deserialise_order(buffer, order);
  clr.type = RequestType::New;
  clr.client_id = cid;
  // TODO: avoid chrono syscalls.
  // Find some better method of accurate time stamps.
  clr.time_stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  clr.new_order = order;
  /* std::cout << "New Order placed with Order id = " << clr.new_order.order_id
            << " client id: " << clr.client_id
            << " price : " << clr.new_order.price
            << " quantity:  " << clr.new_order.quantity << "\n"; */
  event_queue.push(clr);
}

template <TachyonConfig config>
void TcpServer<config>::handleCancellation(uint8_t *buffer, ClientId cid) {
  OrderId order_id_to_cancel = deserialise_order_cancel(buffer);
  ClientRequest clr;
  clr.client_id = cid;
  clr.type = RequestType::Cancel;
  clr.order_id_to_cancel = order_id_to_cancel;
  clr.time_stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  // std::cout << "Cancellation request for order id " << clr.order_id_to_cancel
  //   << " placed by client id " << cid << '\n';
  event_queue.push(clr);
}

template <TachyonConfig config>
auto TcpServer<config>::flushBuffer(
    ClientConnection<typename config::RxBufferType,
                     typename config::TxBufferType> *conn) -> bool {
  if (conn->tx_buffer.size() == 0) {
    return true;
  }
  const uint8_t *data_ptr = conn->tx_buffer.begin() + conn->tx_offset;
  size_t remaining = conn->tx_buffer.size() - conn->tx_offset;

  // non blocking send.
  ssize_t sent = send(conn->fd, data_ptr, remaining, MSG_DONTWAIT);
  if (sent > 0) {
    conn->tx_offset += sent;
  }

  else if (sent == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // kernel buffer full. stop trying.
      std::cout << "Kernel buffer full\n";
      return false;
    }
    // other error? I guess client dead.
    return false;
  }
  // check if sent everything.
  if (conn->tx_offset >= conn->tx_buffer.size()) {
    conn->tx_buffer.clear();
    conn->tx_offset = 0;
    return true;
  }

  // still data left.
  return false;
}

template <TachyonConfig config> void TcpServer<config>::dispatchData() {
  ExecutionReport report{};
  uint8_t serialise_buf[64]; // serialisation buffer.
  while (keep_running.load(std::memory_order_relaxed)) {
    // drain the queue via batch processing.
    int pops = 0;
    bool work_done = false;
    while (pops < 100 && execution_reports.try_pop(report)) {
      work_done = true;
      pops++;
      size_t len = serialise_execution_report(report, serialise_buf);
      {
        if (client_map.contains(report.client_id)) {
          ClientConnection<typename config::RxBufferType,
                           typename config::TxBufferType> *conn =
              client_map.at(report.client_id);

          conn->tx_buffer.insert(serialise_buf, len);
        }
      }
    }
    {
      // flush the buffers.
      // NOTE: since our clients id's are 1 based, we CAN do this.
      // However, needs improvement.
      for (ClientId i = 1; i < next_id.load(); i++) {
        if (client_map.contains(i)) {
          ClientConnection<typename config::RxBufferType,
                           typename config::TxBufferType> *conn =
              client_map.at(i);
          if (!conn->tx_buffer.size() == 0) {
            flushBuffer(conn);
            work_done = true;
            // maybe use some epoll feature if fush buffer returns false??
          }
        }
      }
    }
    if (!work_done) {
      std::this_thread::yield();
    }
  }
}

template class TcpServer<my_config>;
template struct ClientConnection<my_config::RxBufferType,
                                 my_config::TxBufferType>;
