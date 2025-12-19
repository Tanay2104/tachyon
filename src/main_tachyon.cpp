#include "my_config.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <engine/concepts.hpp>
#include <engine/exchange.hpp>
#include <ratio>

std::atomic<bool> keep_running(true);

// Signal handler for Ctrl + C
void signal_handler(int /*unused*/) { keep_running.store(false); }

auto main() -> int {
  // The main entry point of our simulation.
  std::signal(SIGINT, signal_handler);

  uint32_t duration = 500000; // Running duration in ms.
  Exchange<my_config> exchange;
  // exchange.addClients(4);
  exchange.init();
  auto start = std::chrono::steady_clock::now();
  exchange.run();
  auto end = std::chrono::steady_clock::now();
  while (true) {
    end = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count() > duration) {
      exchange.stop();
      break;
    }
  }
}
