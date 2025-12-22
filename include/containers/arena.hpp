#pragma once
#include <cstdint>
#include <engine/types.hpp>

class ArenaClass {
private:
  struct OrderSlot { // NOTE: make sure ABA problem doesn't occur.

    ClientRequest clr;
    bool is_active = false;
  };

  std::vector<OrderSlot> arena;
  std::vector<uint32_t> free_list; // Acts as stack.
public:
  ArenaClass() { arena.reserve(10'000'000); }
  uint32_t allocateSlot(const ClientRequest &incoming) {
    uint32_t idx = 0;
    if (!free_list.empty()) {
      // recycle an old block.
      idx = free_list.back();
      free_list.pop_back();
    } else {
      // create new slot.
      idx = arena.size();
      arena.emplace_back();
    }
    arena[idx].clr = incoming;
    arena[idx].is_active = true;
    return idx;
  }
  void freeSlot(uint32_t idx) {
    assert(arena[idx].is_active);
    arena[idx].is_active = false;
    free_list.push_back(idx);
  }
  OrderSlot &operator[](uint32_t idx) { return arena[idx]; }
};
