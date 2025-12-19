#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <queue>
#include <thread>
#include <vector>

#include "containers/flat_hashmap.hpp"

namespace threadsafe {

// Thread safe queue for Multi Producer single consumer.
template <typename T> class sharded_queue {
private:
  struct comp {
    auto operator()(const std::pair<T, size_t> &a,
                    const std::pair<T, size_t> &b) -> bool {
      return (a.second > b.second); // if a comes later it has lower priority.
    }
  };
  std::vector<std::deque<std::pair<T, size_t>>>
      shards; // shards corresponding to each object.

  std::atomic<size_t>
      times; // the time of entry of each object. Intialised to zero.

  std::priority_queue<std::pair<T, size_t>, std::vector<std::pair<T, size_t>>,
                      comp>
      pq;

  flat_hashmap<std::thread::id, uint16_t>
      map; // map for storing thread id's to index.

  std::atomic<uint16_t> index; // initialised to zero.
public:
  void push(T element) {
    if (!map.contains(std::this_thread::get_id())) {
      map.insert({std::this_thread::get_id(),
                  index++}); // now index - 1 will point to the queue.
      shards.push_back({});
    }
    shards[map.at(std::this_thread::get_id())].push_back({element, times++});
  }
  auto try_pop(T &element) -> bool {
    if (!pq.empty()) {
      element = pq.top().first;
      pq.pop();
      return true;
    }
    for (uint16_t i = 0; i < index; i++) {
      if (!shards[i].empty()) {
        pq.push({shards[i].front()}); // this is giving data race vibes.
        shards[i].pop_front();
      }
    }
    if (pq.empty()) {
      return false;
    }
    element = pq.top().first;
    pq.pop();
    return true;
  }
};

}; // namespace threadsafe
