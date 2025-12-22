#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <cstddef>
// NOTE: These constants are for simulation and testing purposes only.
static constexpr size_t MAX_PROCESSED_EVENTS_SIZE = 100000;
static constexpr size_t MAX_TRADE_BUFFER_SIZE = 1000;
static constexpr size_t MAX_TRADES_QUEUE_SIZE = 100000;
static constexpr size_t MAX_EXECUTION_REPORTS_SIZE = 100000;

static constexpr size_t NUM_DEFAULT_CLIENTS = 4;

static constexpr size_t CLIENT_PRICE_DISTRIB_MIN = (-500);
static constexpr size_t CLIENT_PRICE_DISTRIB_MAX = 500;
static constexpr size_t CLIENT_BASE_PRICE = (10000);

static constexpr size_t CLIENT_QUANTITY_DISTRIB_MIN = (-5000);
static constexpr size_t CLIENT_QUANTITY_DISTRIB_MAX = 5000;
static constexpr size_t CLIENT_BASE_QUANTITY = 50000;
static constexpr size_t LOCAL_ORDER_BITS = 48;

static constexpr size_t ORDER_CANCELLATION_FREQ = 20;
#endif
