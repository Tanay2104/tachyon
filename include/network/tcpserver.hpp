#pragma once
#include <containers/flat_hashmap.hpp>
#include <containers/lock_queue.hpp>
#include <cstddef>
#include <cstdint>
#include <engine/types.hpp>
#include <string>

struct ClientConnection {
  int fd;
  ClientId client_id;

  std::vector<uint8_t> rx_buffer;
  std::vector<uint8_t> tx_buffer;
  size_t tx_offset = 0;  // how much of tx buffer is already sent?
  ClientConnection(int file_descriptor) : fd(file_descriptor) {
    rx_buffer.reserve(1024);
    tx_buffer.reserve(1024);
  }
};

class TcpServer {
 private:
  int listen_fd;  // File descriptor for listening to sockets connections.
  std::atomic<ClientId> next_id;  //  the client Id we need to assign
                                  //  to the incoming new client.
  threadsafe::stl_queue<ClientRequest>& event_queue;
  threadsafe::stl_queue<ExecutionReport>& execution_reports;

  static constexpr int BACKLOG = 20;
  static constexpr int MAX_EPOLL_EVENTS = 10;
  static constexpr int MAX_TEMP_BUFF_SIZE = 4096;
  static void setNonBlocking(int file_descriptor);

  void handleNewConnection(struct epoll_event evt, int epoll_fd);
  void handleNewOrder(uint8_t* buffer, ClientId cid);
  void handleCancellation(uint8_t* buffer, ClientId cid);

  // this works. But is this really the best way?
  flat_hashmap<ClientId, ClientConnection*> client_map;  // for dispatcher.
  std::mutex client_map_mutex;

  auto flushBuffer(ClientConnection* conn)
      -> bool;  // return true if buff empty, all
                // sent. False if socket is full.
 public:
  TcpServer(threadsafe::stl_queue<ClientRequest>& event_queue,
            threadsafe::stl_queue<ExecutionReport>& execution_reports);
  void init(std::string port);
  // NOTE: we use separate file_descriptors and epolls for reading and writing,
  // and assume that the client also has separate read write threads.
  void receiveData();
  void dispatchData();
};
