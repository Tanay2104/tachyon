#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>
template <typename T> class flat_buffer {
private:
  static constexpr uint32_t MAX_CAPACITY = std::numeric_limits<uint32_t>::max();
  static constexpr uint32_t DEFAULT_SIZE = 1024;
  size_t tail;     // Index to next to be inserted element.
  size_t head;     // Index to first element
  size_t capacity; // current capacity.
  T *data;

  void grow() {
    size_t new_capacitity = capacity * 2;
    if (new_capacitity > MAX_CAPACITY) {
      throw std::runtime_error("Cannot grow buffer beyond max capacity.");
    }
    T *tmp = new T[new_capacitity];
    std::memcpy(tmp, &data[head], size());
    delete data;
    data = tmp;
    tail = size();
    head = 0;
    capacity = new_capacitity;
  }

public:
  using value_type = T;

  flat_buffer(int n = DEFAULT_SIZE) : capacity(n), head(0), tail(0) {
    data = new T[capacity];
  }

  size_t size() { return (tail - head); }
  T *begin() { return &data[head]; }
  T *end() { return &data[tail]; }

  // Insert the data stored at start at the end.
  void insert(T *start, size_t count) {
    if (head >= capacity - 1 - size()) { // Buffer is "saturated",
      reset();
    } else if (tail >= capacity - 1) { // ideally it should be  ==
      grow();
    }
    std::memcpy(&data[tail], start, count);
    tail = tail + count;
  }

  // Delete data stored at beginning.
  void erase(size_t count) { head += count; }
  // erase all data.
  void clear() { head = tail = 0; }

  // Move all elements to the beginning.
  void reset() {
    // NOTE: I am assuming that memcpy works sequentially.
    // Otherwise data might become corrupted?
    std::memcpy(data, &data[head], size());
  }
};
