#pragma once
#include <algorithm>
#include <containers/flat_hashmap.hpp>
#include <cstddef>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

// To be used where V is a pointer.

namespace threadsafe {
template <typename Key, typename Value> class hashmap {
private:
  class bucket_type {
  private:
    using bucket_value = std::pair<Key, Value>;
    using bucket_data = std::list<bucket_value>;
    using bucket_iterator = bucket_data::iterator;

    bucket_data data;
    mutable std::shared_mutex mutex;
    bucket_iterator find_entry_for(Key const &key) const {
      return std::find_if(
          data.begin(), data.end(),
          [&](const bucket_value &item) { return item.first == key; });
    }
    bucket_iterator find_entry_for(Key const &key) {
      return std::find_if(
          data.begin(), data.end(),
          [&](const bucket_value &item) { return item.first == key; });
    }

  public:
    Value &at(Key const &key) {
      std::shared_lock<std::shared_mutex> lock(mutex);
      bucket_iterator const found_entry = find_entry_for(key);
      if (found_entry == data.end()) {
        throw std::runtime_error("Requested key is not present in hashmap");
      }
      return found_entry->second;
    }

    void insert(const Key &key, const Value &value) {
      std::unique_lock<std::shared_mutex> lock(mutex);
      bucket_iterator iter = find_entry_for(key);
      if (iter == data.end()) {
        data.push_back({key, value});
      } else {
        iter->second = value;
      }
    }
    void erase(const Key &key) {
      std::unique_lock<std::shared_mutex> lock(mutex);
      bucket_iterator iter = find_entry_for(key);
      if (iter == data.end()) {
        throw std::runtime_error("Cannot erase as key does not exist in map");
      }
      data.erase(iter);
    }
    bool contains(const Key &key) {
      std::shared_lock<std::shared_mutex> lock(mutex);
      return (find_entry_for(key) != data.end());
    }
  };

  std::vector<std::unique_ptr<bucket_type>> buckets;
  bucket_type &get_bucket(Key const &key) const {
    size_t const bucket_index = std::hash<Key>{}(key) % buckets.size();
    return *buckets[bucket_index];
  }

public:
  using mapped_type = Value;
  hashmap(unsigned num_buckets = 16) : buckets(num_buckets) {
    for (uint32_t i = 0; i < num_buckets; i++) {
      buckets[i].reset(new bucket_type);
    }
  }
  Value &at(const Key &key) const { return get_bucket(key).at(key); }
  void insert(std::pair<const Key &, const Value &> pair) {
    return get_bucket(pair.first).insert(pair.first, pair.second);
  }
  bool contains(const Key &key) { return get_bucket(key).contains(key); }
  void erase(const Key &key) { return get_bucket(key).erase(key); }
};
}; // namespace threadsafe
