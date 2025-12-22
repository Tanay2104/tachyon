#pragma once
#include <atomic>
#include <optional>
#include <vector>

template <typename T> class LockFreeSPSCQueue {
private:
  struct Element {
    T value;
  };

  // Cache line size to prevent false sharing
  static constexpr size_t CACHE_LINE = 64;

  // Data alignment
  alignas(CACHE_LINE) std::vector<Element> buffer;
  size_t mask;

  // Head and Tail on separate cache lines
  alignas(CACHE_LINE) std::atomic<size_t> head{0};
  alignas(CACHE_LINE) std::atomic<size_t> tail{0};

public:
  explicit LockFreeSPSCQueue(size_t capacity = 1024 * 1024) {
    // Enforce power of 2 for fast modulo
    size_t cap = 1;
    while (cap < capacity)
      cap *= 2;
    buffer.resize(cap);
    mask = cap - 1;
  }

  // Writer Thread Only
  bool push(const T &item) {
    const size_t t = tail.load(std::memory_order_relaxed);
    const size_t h = head.load(std::memory_order_acquire);

    if (((t + 1) & mask) == (h & mask)) {
      return false; // Full
    }

    buffer[t & mask].value = item;
    tail.store(t + 1, std::memory_order_release);
    return true;
  }

  // Reader Thread Only
  bool try_pop(T &item) {
    const size_t h = head.load(std::memory_order_relaxed);
    const size_t t = tail.load(std::memory_order_acquire);

    if (h == t) {
      return false; // Empty
    }

    item = buffer[h & mask].value;
    head.store(h + 1, std::memory_order_release);
    return true;
  }

  // NOTE: function used only by one thread and only for logging
  size_t size() { return (tail.load() - head.load()) % buffer.capacity(); }
};
