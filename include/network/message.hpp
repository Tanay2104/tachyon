#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "engine/types.hpp"

// Asserts  Only needed during testing just in case I change something and seg
// fault occurs:

static_assert(sizeof(OrderId) == 8);
static_assert(sizeof(Price) == 8);
static_assert(sizeof(Quantity) == 4);
static_assert(sizeof(Side) == 1);
static_assert(sizeof(OrderType) == 1);
static_assert(sizeof(TimeInForce) == 1);
static_assert(sizeof(ClientId) == 4);

enum class MessageType : uint8_t {
  ORDER_NEW = 1,
  ORDER_CANCEL = 2,
  EXEC_REPORT = 3,
  TRADE = 4,
};
// NOLINTBEGIN
// Converts Order struct to 24 byte buffer.
// Returns number of bytes written(ideally always 24)
// Assuming client server same Endianness.
auto serialise_order(const Order& order, uint8_t* buffer) -> size_t {
  // Byte 0 : message type.
  buffer[0] = static_cast<uint8_t>(MessageType::ORDER_NEW);

  // Byte 1-8: order id.
  std::memcpy(&buffer[1], &order.order_id, 8);
  std::memcpy(&buffer[9], &order.price, 8);
  std::memcpy(&buffer[17], &order.quantity, 4);
  buffer[21] = static_cast<uint8_t>(order.side);
  buffer[22] = static_cast<uint8_t>(order.order_type);
  buffer[23] = static_cast<uint8_t>(order.tif);

  return 24;
}

auto deserialise_order(const uint8_t* buffer) -> Order {
  // Sould we assert this? I think so.
  assert(buffer[0] == static_cast<uint8_t>(MessageType::ORDER_NEW));
  Order order{};
  std::memcpy(&order.order_id, &buffer[1], 8);
  std::memcpy(&order.price, &buffer[9], 8);
  std::memcpy(&order.quantity, &buffer[17], 4);
  order.side = static_cast<Side>(buffer[21]);
  order.order_type = static_cast<OrderType>(buffer[22]);
  order.tif = static_cast<TimeInForce>(buffer[23]);
  return order;
}

auto serialise_execution_report(const ExecutionReport& report,
                                uint8_t* buffer) {
  buffer[0] = static_cast<uint8_t>(MessageType::EXEC_REPORT);
  std::memcpy(&buffer[1], &report.client_id, 4);
  std::memcpy(&buffer[5], &report.order_id, 8);
  std::memcpy(&buffer[13], &report.price, 8);
  std::memcpy(&buffer[21], &report.last_quantity, 4);
  std::memcpy(&buffer[25], &report.remaining_quantity, 4);
  buffer[29] = static_cast<uint8_t>(report.type);
  buffer[30] = static_cast<uint8_t>(report.reason);
  buffer[31] = static_cast<uint8_t>(report.side);

  return 32;
}

auto deserialise_execution_report(const uint8_t* buffer) -> ExecutionReport {
  ExecutionReport report{};
  assert(buffer[0] == static_cast<uint8_t>(MessageType::EXEC_REPORT));
  std::memcpy(&report.client_id, &buffer[1], 4);
  std::memcpy(&report.order_id, &buffer[5], 8);
  std::memcpy(&report.price, &buffer[13], 8);
  std::memcpy(&report.last_quantity, &buffer[21], 4);
  std::memcpy(&report.remaining_quantity, &buffer[25], 4);
  report.type = static_cast<ExecType>(buffer[29]);
  report.reason = static_cast<RejectReason>(buffer[30]);
  report.side = static_cast<Side>(buffer[31]);
  return report;
}

auto serialise_trade(const Trade& trade, uint8_t* buffer) -> size_t {
  buffer[0] = static_cast<uint8_t>(MessageType::TRADE);

  std::memcpy(&buffer[1], &trade.maker_order_id, 8);
  std::memcpy(&buffer[9], &trade.taker_order_id, 8);
  std::memcpy(&buffer[17], &trade.time_stamp, 8);
  std::memcpy(&buffer[25], &trade.price, 8);
  std::memcpy(&buffer[33], &trade.quantity, 4);

  buffer[37] = static_cast<uint8_t>(trade.aggressor_side);

  return 38;
}

auto deserialise_trade(const uint8_t* buffer) -> Trade {
  assert(buffer[0] == static_cast<uint8_t>(MessageType::TRADE));
  Trade trade{};
  std::memcpy(&trade.maker_order_id, &buffer[1], 8);
  std::memcpy(&trade.taker_order_id, &buffer[9], 8);
  std::memcpy(&trade.time_stamp, &buffer[17], 8);
  std::memcpy(&trade.price, &buffer[25], 8);
  std::memcpy(&trade.quantity, &buffer[33], 4);

  trade.aggressor_side = static_cast<Side>(buffer[37]);

  return trade;
}

auto serialize_order_cancel(ClientId client_id, OrderId order_id,
                            uint8_t* buffer) -> size_t {
  buffer[0] = static_cast<uint8_t>(MessageType::ORDER_CANCEL);

  std::memcpy(&buffer[1], &client_id, 4);
  std::memcpy(&buffer[5], &order_id, 8);

  return 13;
}

auto deserialize_order_cancel(const uint8_t* buffer)
    -> std::pair<ClientId, OrderId> {
  ClientId client_id;
  OrderId order_id;

  std::memcpy(&client_id, &buffer[1], 4);
  std::memcpy(&order_id, &buffer[5], 8);

  return {client_id, order_id};
}

// NOLINTEND
