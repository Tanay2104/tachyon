#include <benchmark/benchmark.h>

#include <cstdint>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

#include "containers/flat_hashmap.hpp"

// ============================================================
// Types
// ============================================================

using Key = uint64_t;

struct Value48 {
  uint64_t a, b, c, d, e, f;
};

// ============================================================
// Utilities
// ============================================================

static std::vector<Key> make_keys(size_t n) {
  std::vector<Key> keys(n);
  for (size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<Key>(i * 11400714819323198485ULL);
  }
  return keys;
}

static std::vector<Value48> make_values(size_t n) {
  std::vector<Value48> vals(n);
  for (size_t i = 0; i < n; ++i) {
    vals[i] = Value48{i, i + 1, i + 2, i + 3, i + 4, i + 5};
  }
  return vals;
}

// ============================================================
// IDEAL INSERT (NO DELETIONS)
// ============================================================

static void BM_Flat_Insert_Ideal(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  for (auto _ : state) {
    flat_hashmap<Key, Value48> map(N * 2);
    for (size_t i = 0; i < N; ++i) {
      map.insert({keys[i], vals[i]});
    }
    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * N);
}

static void BM_Unordered_Insert_Ideal(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  for (auto _ : state) {
    std::unordered_map<Key, Value48> map;
    map.reserve(N);
    for (size_t i = 0; i < N; ++i) {
      map.emplace(keys[i], vals[i]);
    }
    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * N);
}

static void BM_Map_Insert_Ideal(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  for (auto _ : state) {
    std::map<Key, Value48> map;
    for (size_t i = 0; i < N; ++i) {
      map.emplace(keys[i], vals[i]);
    }
    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * N);
}

// ============================================================
// IDEAL LOOKUP (NO DELETIONS)
// ============================================================

static void BM_Flat_Lookup_Ideal(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  flat_hashmap<Key, Value48> map(N * 2);
  for (size_t i = 0; i < N; ++i)
    map.insert({keys[i], vals[i]});

  for (auto _ : state) {
    for (size_t i = 0; i < N; ++i) {
      benchmark::DoNotOptimize(map.at(keys[i]));
    }
  }

  state.SetItemsProcessed(state.iterations() * N);
}
static void BM_Unordered_Lookup_Ideal(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  std::unordered_map<Key, Value48> map(N * 2);
  for (size_t i = 0; i < N; ++i)
    map.insert({keys[i], vals[i]});

  for (auto _ : state) {
    for (size_t i = 0; i < N; ++i) {
      benchmark::DoNotOptimize(map.at(keys[i]));
    }
  }

  state.SetItemsProcessed(state.iterations() * N);
}

static void BM_Map_Lookup_Ideal(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  std::map<Key, Value48> map;
  for (size_t i = 0; i < N; ++i)
    map.insert({keys[i], vals[i]});

  for (auto _ : state) {
    for (size_t i = 0; i < N; ++i) {
      benchmark::DoNotOptimize(map.at(keys[i]));
    }
  }

  state.SetItemsProcessed(state.iterations() * N);
}
// ============================================================
// DELETE COST (TOMBSTONE CREATION)
// ============================================================

static void BM_Flat_erase(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  for (auto _ : state) {
    flat_hashmap<Key, Value48> map(N * 2);
    for (size_t i = 0; i < N; ++i)
      map.insert({keys[i], vals[i]});

    for (size_t i = 0; i < N / 2; ++i)
      map.erase(keys[i]);

    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * (N / 2));
}
static void BM_Unordered_erase(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  for (auto _ : state) {
    std::unordered_map<Key, Value48> map(N * 2);
    for (size_t i = 0; i < N; ++i) {
      map.insert({keys[i], vals[i]});
    }

    for (size_t i = 0; i < N / 2; ++i)
      map.erase(keys[i]);

    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * (N / 2));
}

static void BM_Map_erase(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N);
  auto vals = make_values(N);

  for (auto _ : state) {
    std::map<Key, Value48> map;
    for (size_t i = 0; i < N; ++i)
      map.insert({keys[i], vals[i]});

    for (size_t i = 0; i < N / 2; ++i)
      map.erase(keys[i]);

    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * (N / 2));
}

// ============================================================
// INSERT AFTER DELETE (TOMBSTONE REUSE)
// ============================================================

static void BM_Flat_Insert_After_Delete(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N * 2);
  auto vals = make_values(N * 2);

  for (auto _ : state) {
    flat_hashmap<Key, Value48> map(N * 2);

    for (size_t i = 0; i < N; ++i)
      map.insert({keys[i], vals[i]});

    for (size_t i = 0; i < N / 2; ++i)
      map.erase(keys[i]);

    for (size_t i = N; i < N + N / 2; ++i)
      map.insert({keys[i], vals[i]});

    benchmark::DoNotOptimize(map);
  }

  state.SetItemsProcessed(state.iterations() * (N / 2));
}

// ============================================================
// MIXED STEADY-STATE WORKLOAD
// ============================================================

static void BM_Flat_Mixed(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N * 4);
  auto vals = make_values(N * 4);

  flat_hashmap<Key, Value48> map(N * 2);
  std::vector<Key> live_keys;
  live_keys.reserve(N);

  // Initial population
  for (size_t i = 0; i < N; ++i) {
    map.insert({keys[i], vals[i]});
    live_keys.push_back(keys[i]);
  }

  size_t next_insert = N;
  std::mt19937_64 rng(123456);

  for (auto _ : state) {
    uint64_t r = rng() % 100;

    if (r < 50 && !live_keys.empty()) {
      // Lookup
      Key k = live_keys[rng() % live_keys.size()];
      benchmark::DoNotOptimize(map.at(k));
    } else if (r < 80 && next_insert < keys.size()) {
      // Insert
      Key k = keys[next_insert];
      map.insert({k, vals[next_insert]});
      live_keys.push_back(k);
      ++next_insert;
    } else if (!live_keys.empty()) {
      // Remove
      size_t idx = rng() % live_keys.size();
      map.erase(live_keys[idx]);
      live_keys[idx] = live_keys.back();
      live_keys.pop_back();
    }
  }

  state.SetItemsProcessed(state.iterations());
}

static void BM_Unordered_Mixed(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N * 4);
  auto vals = make_values(N * 4);

  std::unordered_map<Key, Value48> map;
  std::vector<Key> live_keys;
  live_keys.reserve(N);

  // Initial population
  for (size_t i = 0; i < N; ++i) {
    map.insert({keys[i], vals[i]});
    live_keys.push_back(keys[i]);
  }

  size_t next_insert = N;
  std::mt19937_64 rng(123456);

  for (auto _ : state) {
    uint64_t r = rng() % 100;

    if (r < 50 && !live_keys.empty()) {
      // Lookup
      Key k = live_keys[rng() % live_keys.size()];
      benchmark::DoNotOptimize(map.at(k));
    } else if (r < 80 && next_insert < keys.size()) {
      // Insert
      Key k = keys[next_insert];
      map.insert({k, vals[next_insert]});
      live_keys.push_back(k);
      ++next_insert;
    } else if (!live_keys.empty()) {
      // Remove
      size_t idx = rng() % live_keys.size();
      map.erase(live_keys[idx]);
      live_keys[idx] = live_keys.back();
      live_keys.pop_back();
    }
  }

  state.SetItemsProcessed(state.iterations());
}

static void BM_Map_Mixed(benchmark::State &state) {
  const size_t N = state.range(0);
  auto keys = make_keys(N * 4);
  auto vals = make_values(N * 4);

  std::map<Key, Value48> map;
  std::vector<Key> live_keys;
  live_keys.reserve(N);

  // Initial population
  for (size_t i = 0; i < N; ++i) {
    map.insert({keys[i], vals[i]});
    live_keys.push_back(keys[i]);
  }

  size_t next_insert = N;
  std::mt19937_64 rng(123456);

  for (auto _ : state) {
    uint64_t r = rng() % 100;

    if (r < 50 && !live_keys.empty()) {
      // Lookup
      Key k = live_keys[rng() % live_keys.size()];
      benchmark::DoNotOptimize(map.at(k));
    } else if (r < 80 && next_insert < keys.size()) {
      // Insert
      Key k = keys[next_insert];
      map.insert({k, vals[next_insert]});
      live_keys.push_back(k);
      ++next_insert;
    } else if (!live_keys.empty()) {
      // Remove
      size_t idx = rng() % live_keys.size();
      map.erase(live_keys[idx]);
      live_keys[idx] = live_keys.back();
      live_keys.pop_back();
    }
  }

  state.SetItemsProcessed(state.iterations());
}
// ============================================================
// REGISTRATION
// ============================================================

BENCHMARK(BM_Flat_Insert_Ideal)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Unordered_Insert_Ideal)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(32768)
    ->Arg(262144);
BENCHMARK(BM_Map_Insert_Ideal)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);

BENCHMARK(BM_Flat_Lookup_Ideal)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Unordered_Lookup_Ideal)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(32768)
    ->Arg(262144);
BENCHMARK(BM_Map_Lookup_Ideal)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Flat_erase)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Unordered_erase)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Map_erase)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Flat_Insert_After_Delete)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(32768)
    ->Arg(262144);
BENCHMARK(BM_Flat_Mixed)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Unordered_Mixed)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);
BENCHMARK(BM_Map_Mixed)->Arg(1024)->Arg(4096)->Arg(32768)->Arg(262144);

BENCHMARK_MAIN();
