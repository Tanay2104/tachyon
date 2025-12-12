#ifndef LOCK_QUEUE_HPP
#define LOCK_QUEUE_HPP

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <ostream>

namespace threadsafe {
template <typename T>
class lock_queue {
 private:
  T* A;  // Contiguous array to store the elements.
  size_t N;
  size_t head;
  size_t tail;
  static const uint8_t MULTIPLIER = 2;
  static const uint8_t INIT_SIZE = 4;

  mutable std::mutex mut;  // the mutex must be mutable
  std::condition_variable cv;
  // Helper function to increase size
  void grow() {
    size_t new_N = N * MULTIPLIER;
    T* tmp = new T[new_N];
    for (size_t i = 0; i < N; ++i) {
      tmp[i] = A[(head + i) % N];  // This is slow but correct logic
    }
    delete[] A;
    tail = N - 1;
    head = 0;
    // tail = head + size();
    A = tmp;
    N = new_N;
  }

 public:
  // Constructor for queue
  lock_queue() {
    A = new T[INIT_SIZE];
    N = INIT_SIZE;
    head = tail = 0;
  }

  lock_queue(const lock_queue<T>& other) {
    std::lock_guard<std::mutex> lg(other.mut);
    this->A = new T[other.N];
    this->N = other.N;
    for (size_t i = 0; i < N; i++) {
      this->A[(head + i) % N] = other.A[(head + i) % N];
    }
    this->head = other.head;
    this->tail = other.tail;
  }

  ~lock_queue() { delete[] A; }

  bool empty() {
    std::lock_guard<std::mutex> lg(mut);
    return (head == tail);
  }

  size_t size() {
    std::lock_guard<std::mutex> lg(mut);
    return (N + tail - head) % N;
  }

  void push(T x) {
    // grow if full
    // NOTE: Only one thread can call this at a time. This implies that grow()
    // function can be called by only one thread at a time.
    std::lock_guard<std::mutex> lg(mut);
    if (head == (tail + 1) % N) grow();
    A[tail] = x;
    tail = (tail + 1) % N;
    cv.notify_one();  // In case any thread was waiting, notify that thread.
  }

  bool try_pop(T& x) {
    std::lock_guard<std::mutex> lg(mut);
    if (head == tail) return false;
    x = A[head];
    head = (head + 1) % N;
    return true;
  }

  void wait_pop(T& x) {
    std::unique_lock<std::mutex> lg(mut);
    cv.wait(lg, [this]() { return (this->head != this->tail); });
    x = A[head];
    head = (head + 1) % N;
  }
};
}  // namespace threadsafe

namespace threadsafe {
template <typename T>
class stl_queue {
 private:
  mutable std::mutex mut;
  std::deque<T> data_queue;
  std::condition_variable cv;

 public:
  stl_queue() = default;
  stl_queue(const stl_queue<T>& other) {
    std::lock_guard<std::mutex> lk(other.mut);
    data_queue = other.data_queue;
  }
  void push(T x) {
    std::lock_guard<std::mutex> lk(mut);
    data_queue.push_back(x);
    cv.notify_one();
  }
  void wait_pop(T& x) {
    std::unique_lock<std::mutex> lk(mut);
    cv.wait(lk, [this]() { !this->data_queue.empty(); });
    x = data_queue.front();
    data_queue.pop();
  }
  bool try_pop(T& x) {
    std::lock_guard<std::mutex> lk(mut);
    if (data_queue.empty()) return false;
    x = data_queue.front();
    data_queue.pop_front();
    return true;
  }
  bool empty() {
    std::lock_guard<std::mutex> lk(mut);
    return data_queue.empty();
  }
  size_t size() {
    std::lock_guard<std::mutex> lk(mut);
    // if (data_queue.size() % 100 == 0) std::cout << "queue size: " <<
    // data_queue.size() << std::endl;
    return data_queue.size();
  }
};
}  // namespace threadsafe

#endif
