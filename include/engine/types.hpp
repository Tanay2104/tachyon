#ifndef TYPES_HPP
#define TYPES_HPP
#include <cstdint>

#include "containers/intrusive_list.hpp"
#include "containers/lock_queue.hpp"
using OrderId = uint32_t;
using ClientId = uint32_t;
using Price = uint64_t;      // Precision upto four decimal points supported.
                             // For example, $12.9224 is represented as 129224.
using Quantity = uint32_t;   // Larger quantity may be needed, let's see later.
using TimeStamp = uint64_t;  // Time in nanoseconds since start of trading day.

template <typename T>
using queue = threadsafe::stl_queue<T>;
enum class RequestType : uint8_t { New, Cancel };
enum class Side : uint8_t {
  BID,  // Highest price a buyer is willing to pay
  ASK   // Lowest price a seller will accept.
};
enum class OrderType : uint8_t {
  LIMIT,  // Execute only when the market price reaches specified limit
          // price or better.
  MARKET  // Execute at best available price.
};
enum class TimeInForce : uint8_t {
  GTC,  // Stands for good till cancelled, they stay active until executed or
        // manually cancelled. Can be partially filled, remainder stays active
  IOC   // Stands for immediate or cancelled, execute instantly for best
        // available price and canceling any unfilled portion.
};

// NOTE: All combinations between OrderType and TimeInForce are valid. However,
// market x GTC is not served as market orders are inherently immediate.

// TODO: check out # pragma pack(1) or __attribute__((packed)) or use any other
// method to improve packing and performance.

// The main order struct.
struct Order {
  OrderId order_id;      // 4 bytes
  Price price;           // 8 bytes
  Quantity quantity;     // 4 bytes
  Side side;             // 1 byte
  OrderType order_type;  // 1 byte
  TimeInForce tif;       // 1 byte
};  // Total 19 bytes.

struct ClientRequest {
  RequestType type;              // 1 byte
  ClientId client_id;            // 4 bytes
  TimeStamp time_stamp;          // 8 bytes
  IntrusiveListNode intr_node;   // 16 bytes!!!!
  union {                        // Union saves memory.
    Order new_order;             // 19 bytes
    OrderId order_id_to_cancel;  // 4 bytes
  };
};
// The size of a union is size of largest element.
// Hence the size of client request is 14 + 19 = 32 bytes.
// TODO: Wrong prev statement: Perfect two objects in a cache line without any
// space waste.

enum class ExecType : uint8_t {
  NEW = 0,       // Order accepted
  CANCELED = 1,  // Order successfully cancelled.
  REJECTED = 2,  // Order rejected. Could be invalid price, quantity.
  TRADE = 3,     // Partial or full fill.
  EXPIRED = 4,   // IOC expired.
};

enum class RejectReason : uint8_t {
  NONE = 0,
  ORDER_NOT_FOUND = 1,
  PRICE_INVALID = 2,
  QUANTITY_INVALID = 3,
  MARKET_CLOSED = 4,
  SELF_TRADE = 5,
  INVALID_ORDER_TYPE = 6
};

// Execution report sent to the client regarding the order.
struct ExecutionReport {
  ClientId client_id;
  OrderId order_id;
  Price price;                  // Last price filled or 0
  Quantity last_quantity;       // Quantity filled in this event.
  Quantity remaining_quantity;  // Remaining quanity if any.
  ExecType type;                // What happened?
  RejectReason reason;          // If rejected, why?
  Side side;  // Context, though shouldn't the client know themselves?
};
// Size = 4 + 4 +  8 + 4 + 4 + 3 = 27 bytes.
// NOTE: We are not putting timestamp here so it that it fits in ~32 bytes.
// Anyways client can pull timestamp from public trades queue if they need it.

// The internal data generated when a match happens.
struct Trade {
  OrderId maker_order_id;  // ID of resting order.
  OrderId taker_order_id;  // ID of incoming order.
  TimeStamp time_stamp;
  Price price;
  Quantity quantity;
  Side aggressor_side;  // Who initiated?
};
// Size = 4 + 4 + 8 + 8 + 4 + 1 = 29 bytes.
// Not too much space wasted.
#endif
