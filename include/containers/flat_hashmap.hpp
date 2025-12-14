#ifndef FLAT_HASHMAP_HPP
#define FLAT_HASHMAP_HPP
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

static constexpr size_t DEFAULT_SIZE = 1024;
static constexpr size_t NO_SIZE = std::numeric_limits<size_t>::max();
static constexpr size_t MAX_SIZE = std::numeric_limits<uint32_t>::max();

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
  Entry<K, V>* data;

  auto hashValue(K key) -> size_t { return std::hash<K>{}(key); }

  // Linear probing.
  auto findFreeIndex(K key, Entry<K, V>* arr) -> size_t {
    size_t hash = hashValue(key);
    for (size_t i = 0; i < capacity_; i++) {
      size_t index = (hash + i) % capacity_;
      if ((arr[index].state == State::FREE) ||
          (arr[index].state == State::TOMBSTONE)) {
        return index;
      }
      if ((arr[index].state == State::USED) && (arr[index].key == key)) {
        return index;
      }
    }
    return NO_SIZE;  // Underflows.
  }

  // Function for rehashing and growing.
  void grow() {
    size_t old_N = capacity_;
    capacity_ *= 2;
    if (capacity_ >= MAX_SIZE) {
      throw std::runtime_error(
          "Cannot increase hash table size. Max capacity reached.");
    }
    auto* tmp = new Entry<K, V>[capacity_];
    for (size_t i = 0; i < old_N; i++) {
      if (data[i].state == State::USED) {
        insert(data[i].key, data[i].value, tmp);
      }
    }
    delete[] data;
    data = tmp;
    tombstones_ = 0;
    // size remains same.
  }
  void insert(const K& key, const V& value, Entry<K, V>* arr) {
    if (size_ > 0.7 * capacity_) {
      grow();
      arr = data;
    } else if (tombstones_ > 0.3 * capacity_) {
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
    for (size_t i = 0; i < capacity_; i++) {
      size_t index = (hash + i) % capacity_;
      if (arr[index].state == State::FREE) {
        // If we got free here this means this value hasn't been claimed yet nor
        // tombstoned. So it must be wrong.
        throw std::out_of_range("Value with given key does not exist.");
      }
      if ((arr[index].state != State::TOMBSTONE) && arr[index].key == key) {
        return arr[index].value;
      }
    }
    throw std::out_of_range("Value with given key does not exist.");
  }
  void remove(const K& key, Entry<K, V>* arr) {
    size_t hash = hashValue(key);
    for (size_t i = 0; i < capacity_; i++) {
      size_t index = (hash + i) % capacity_;
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
    }
  }

 public:
  flat_hashmap(size_t n = DEFAULT_SIZE) : capacity_(n) {
    this->data = new Entry<K, V>[capacity_];
  }
  ~flat_hashmap() { delete[] this->data; }
  void insert(const K& key, const V& value) { insert(key, value, data); }

  auto get(const K& key) -> V& { return get(key, data); }

  void remove(const K& key) { remove(key, data); }
};

#endif
