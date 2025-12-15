#ifndef FLAT_HASHMAP_HPP
#define FLAT_HASHMAP_HPP
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
// CRITICAL NOTE: In Debug mode this is resulting in heap buffer overflow erorr
// in testflatmap, by address sanitiser. However, no seg faults.
static constexpr size_t DEFAULT_SIZE = 32768;
static constexpr size_t NO_SIZE = std::numeric_limits<size_t>::max();
static constexpr size_t MAX_SIZE = std::numeric_limits<uint32_t>::max();
static constexpr uint64_t GOLDEN_RATIO = 0x9E3779B97F4A7C15ULL;
enum class State : uint8_t { FREE, TOMBSTONE, USED };

template <typename K, typename V>
struct Entry {
  K key{};
  V value{};
  State state;
  Entry() { state = State::FREE; }
};

template <typename K, typename V>
class flat_hashmap {
 private:
  size_t capacity_{};  // size of array data.
  size_t size_{};
  size_t tombstones_{};
  size_t mask_;
  Entry<K, V>* data;

  auto hashValue(K key) -> size_t {
    if constexpr (std::is_integral_v<K>) {
      return (key * GOLDEN_RATIO);
    }
    return std::hash<K>{}(key);
  }

  static auto next_power_of_two(size_t n) -> size_t {
    if (n == 0) {
      return 1;
    }
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
  }
  // auto hashValue(uint64_t key) -> size_t { }
  // Linear probing.
  auto findFreeIndex(K key, Entry<K, V>* arr) -> size_t {
    size_t hash = hashValue(key);
    size_t index = hash & mask_;
    for (size_t i = 0; i < capacity_; i++) {
      if ((arr[index].state == State::FREE) ||
          (arr[index].state == State::TOMBSTONE)) {
        return index;
      }
      if ((arr[index].state == State::USED) && (arr[index].key == key)) {
        return index;
      }
      index = (index + 1) & mask_;
    }
    return NO_SIZE;  // Underflows.
  }

  // Function for rehashing and growing.
  void grow() {
    size_t old_N = capacity_;
    capacity_ *= 2;
    mask_ = capacity_ - 1;
    if (capacity_ >= MAX_SIZE) {
      tombstones_ = 0;
      throw std::runtime_error(
          "Cannot increase hash table size. Max capacity reached.");
    }
    size_ = 0;
    tombstones_ = 0;
    auto* tmp = new Entry<K, V>[capacity_];
    for (size_t i = 0; i < old_N; i++) {
      if (data[i].state == State::USED) {  // inlined the grow logic.
        size_t index = findFreeIndex(data[i].key, tmp);
        tmp[index].key = data[i].key;
        tmp[index].value = data[i].value;
        tmp[index].state = State::USED;
        size_++;
      }
    }
    delete[] data;
    data = tmp;
    // size remains same.
  }
  void insert(const K& key, const V& value, Entry<K, V>* arr) {
    if (size_ > 0.8 * capacity_) {
      // std::cout << "Growing due to size_ = " << size_ << "\n";
      grow();
      arr = data;
    } else if (tombstones_ > 0.4 * capacity_) {
      // std::cout << "Growing due to tombstones_ = " << tombstones_ << "\n";
      grow();
      arr = data;
    }
    size_t index = findFreeIndex(key, arr);
    // NOTE: we always store a copy of the data.
    if (index == NO_SIZE) {
      // Hash table full.
      grow();
      arr = data;
      index = findFreeIndex(key, arr);
      // return false;
    }
    // if overwriting, delete old key.
    if (arr[index].state == State::FREE) {
      size_++;  // if the state was free we are increasing size.
    }
    if (arr[index].state == State::TOMBSTONE) {
      tombstones_--;  // We no longer have a tombstone.
      size_++;        // but size increases.
    }
    // Do not increase size if used.
    arr[index].key = key;
    arr[index].value = value;
    arr[index].state = State::USED;
  }
  auto get(const K& key, Entry<K, V>* arr) -> V& {  // Value can be modified.
    size_t hash = hashValue(key);

    size_t index = hash & mask_;
    for (size_t i = 0; i < capacity_; i++) {
      if (arr[index].state == State::FREE) {
        // If we got free here this means this value hasn't been claimed yet nor
        // tombstoned. So it must be wrong.
        throw std::out_of_range("Value with given key does not exist.");
      }
      if ((arr[index].state != State::TOMBSTONE) && arr[index].key == key) {
        return arr[index].value;
      }
      index = (index + 1) & mask_;
    }
    throw std::out_of_range("Value with given key does not exist.");
  }
  void remove(const K& key, Entry<K, V>* arr) {
    size_t hash = hashValue(key);

    size_t index = (hash)&mask_;
    for (size_t i = 0; i < capacity_; i++) {
      if (arr[index].state == State::FREE) {  // Same rational as above.
        throw std::out_of_range(
            "Value with given key does not exist and hence cannot be removed.");
      }
      if ((arr[index].state != State::TOMBSTONE) && (arr[index].key == key)) {
        arr[index].state = State::TOMBSTONE;  // Can be used later now.
        tombstones_++;
        size_--;
        return;
      }
      index = (index + 1) & mask_;
    }
  }

 public:
  flat_hashmap(size_t n = DEFAULT_SIZE) {
    capacity_ = next_power_of_two(n);
    this->data = new Entry<K, V>[capacity_];
    mask_ = capacity_ - 1;
  }
  ~flat_hashmap() { delete[] this->data; }
  void insert(const K& key, const V& value) { insert(key, value, data); }

  auto get(const K& key) -> V& { return get(key, data); }

  void remove(const K& key) { remove(key, data); }

  auto contains(const K& key) -> bool {
    size_t hash = hashValue(key);

    size_t index = (hash)&mask_;
    for (size_t i = 0; i < capacity_; i++) {
      if (data[index].state == State::FREE) {
        // If we got free here this means this value hasn't been claimed yet nor
        // tombstoned. So it must be wrong.
        return false;
      }
      if ((data[index].state != State::TOMBSTONE) && data[index].key == key) {
        return true;
      }
      index = (index + 1) & mask_;
    }
    return false;
  }
};

#endif
