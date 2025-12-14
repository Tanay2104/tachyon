#ifndef FLAT_HASHMAP_HPP
#define FLAT_HASHMAP_HPP
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <stdexcept>

#define DEFAULT_SIZE 10000

enum class State : uint8_t { FREE, TOMBSTONE, USED };

template <typename K, typename V>
struct Entry {
  K key;
  V value;
  State state;
};

template <typename K, typename V>
class flat_hashmap {
 private:
  size_t N;  // size of array data.
  Entry<K, V>* data;

  auto hashValue(K key) -> size_t { return std::hash<K>{}(key); }

  // Linear probing.
  auto findFreeIndex(K key) -> size_t {
    size_t hash = hashValue(key);
    for (size_t i = 0; i < N; i++) {
      size_t index = (hash + i) % N;
      if ((data[index].state == State::FREE) ||
          (data[index].state == State::TOMBSTONE)) {
        return index;
      }
      if ((data[index].state == State::USED) && (data[index].key == key)) {
        return index;
      }
    }
    return -1;  // Underflows.
  }

 public:
  flat_hashmap(size_t n = DEFAULT_SIZE) : N(n) {
    this->data = new Entry<K, V>[N];
    for (size_t i = 0; i < N; i++) {
      data[i].state = State::FREE;
    }
  }
  ~flat_hashmap() { delete[] this->data; }
  auto insert(K& key, V& value) -> bool {
    size_t index = findFreeIndex(key);
    // NOTE: we always store a copy of the data.
    if (index == std::numeric_limits<size_t>::max()) {
      // Hash table full.
      return false;
      // TODO: decide some method to increase size?
    }
    // if overwriting, delete old key.
    data[index].key = key;
    data[index].value = value;
    data[index].state = State::USED;

    return true;
  }
  auto get(K key) -> V& {  // Value can be modified.
    size_t hash = hashValue(key);
    for (size_t i = 0; i < N; i++) {
      size_t index = (hash + i) % N;
      if (data[index].state == State::FREE) {
        // If we got free here this means this value hasn't been claimed yet nor
        // tombstoned. So it must be wrong.
        throw std::out_of_range("Value with given key does not exist.");
      }
      if ((data[index].state != State::TOMBSTONE) && data[index].key == key) {
        return data[index].value;
      }
    }
    throw std::out_of_range("Value with given key does not exist.");
  }

  void remove(K key) {
    size_t hash = hashValue(key);
    for (size_t i = 0; i < N; i++) {
      size_t index = (hash + i) % N;
      if (data[index].state == State::FREE) {  // Same rational as above.
        throw std::out_of_range(
            "Value with given key does not exist and hence cannot be removed.");
      }
      if ((data[index].state != State::TOMBSTONE) && (data[index].key == key)) {
        data[index].state = State::TOMBSTONE;  // Can be used later now.
        return;
      }
    }
  }
};

#endif
