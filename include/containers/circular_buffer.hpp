#ifndef CIRCULAR_BUFFER_HPP
#define CIRCULAR_BUFFER_HPP

#include <algorithm>  // For std::copy
#include <cstddef>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#define INIT_SIZE 100000

template <typename T>
class circular_buffer {
 private:
  T* data;
  size_t N{};
  size_t head{};
  size_t tail{};

 public:
  // Default constructor
  explicit circular_buffer(size_t n = INIT_SIZE) : N(n) {
    // Prevent creating a useless 0-sized buffer manually
    if (N == 0) {
      throw std::invalid_argument("Buffer size must be greater than 0");
    }
    data = new T[N];
  }

  // Destructor
  ~circular_buffer() { delete[] data; }

  // Deep copy constructor
  circular_buffer(const circular_buffer& other)
      : N(other.N), head(other.head), tail(other.tail) {
    data = new T[N];
    std::copy(other.data, other.data + N, data);
  }

  // Deep copy assignment
  auto operator=(const circular_buffer& other) -> circular_buffer& {
    if (this == &other) {
      return *this;
    }
    // Strong exception guarantee
    T* new_data = new T[other.N];
    std::copy(other.data, other.data + other.N, new_data);

    delete[] data;
    data = new_data;
    N = other.N;
    head = other.head;
    tail = other.tail;

    return *this;
  }

  // Move constructor
  circular_buffer(circular_buffer&& other) noexcept : data(nullptr) {
    *this = std::move(other);
  }

  // Move assignment
  auto operator=(circular_buffer&& other) noexcept -> circular_buffer& {
    if (this == &other) {
      return *this;
    }
    delete[] data;

    data = std::exchange(other.data, nullptr);
    N = std::exchange(other.N, 0);
    head = std::exchange(other.head, 0);
    tail = std::exchange(other.tail, 0);
    return *this;
  }

  [[nodiscard]] auto empty() const -> bool {
    // tail is always kept < N, so 'tail % N' is redundant.
    // removing % N prevents div-by-zero crash when N=0.
    return head == tail;
  }

  [[nodiscard]] auto full() const -> bool {
    if (N == 0) {
      return true;  // 0 capacity means always full
    }
    return (head == (tail + 1) % N);
  }

  [[nodiscard]] auto size() const -> size_t {
    if (N == 0) {
      return 0;
    }
    return (N + tail - head) % N;
  }

  void push(T&& element) {
    if (N == 0) {
      return;  // Safety check for moved-from objects
    }

    if (full()) {
      head = (head + 1) % N;
    }
    data[tail] = std::move(element);
    tail = (tail + 1) % N;
  }

  void push(const T& element) {
    if (N == 0) {
      return;  // Safety check
    }

    if (full()) {
      head = (head + 1) % N;
    }
    data[tail] = element;
    tail = (tail + 1) % N;
  }

  void pop(T& element) {
    if (empty()) {
      return;
    }
    // std::move out if T supports it
    element = std::move(data[head]);
    head = (head + 1) % N;
  }

  auto operator[](size_t index) -> T& {
    if (size() == 0 || index >= size()) {
      throw std::out_of_range("Out of bounds access");
    }
    return data[(head + index) % N];
  }

  // Const overload for testing
  auto operator[](size_t index) const -> const T& {
    if (size() == 0 || index >= size()) {
      throw std::out_of_range("Out of bounds access");
    }
    return data[(head + index) % N];
  }

  void dump(const std::string& filename,
            std::function<void(T&, std::ofstream&)> how,
            std::ios::openmode mode = std::ios::app) {
    if (N == 0) {
      return;
    }
    std::ofstream file;
    file.open(filename, mode);
    if (!file.is_open()) {
      return;
    }

    for (size_t i = 0; i < size(); i++) {
      how(data[(head + i) % N], file);
    }
    file.close();
  }

  void clear() { head = tail = 0; }
};

#endif  // CIRCULAR_BUFFER_HPP
