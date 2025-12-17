#pragma once

#include <endian.h>

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
  LOGIN_RESPONSE = 5
};
// NOLINTBEGIN
// Converts Order struct to 24 byte buffer.
// Returns number of bytes written(ideally always 24)
// Assuming client server same Endianness.
auto serialise_order(const Order& order, uint8_t* buffer) -> size_t {
  // Byte 0 : message type.
  buffer[0] = static_cast<uint8_t>(MessageType::ORDER_NEW);

  Order network_order;
  network_order.order_id = htobe64(order.order_id);
  network_order.price = htobe64(order.price);
  network_order.quantity = htobe32(order.quantity);
  network_order.side =
      order.side;  // Since this is a single byte no endiannes change needed.
  network_order.order_type = order.order_type;
  network_order.tif = order.tif;

  std::memcpy(&buffer[1], &network_order, sizeof(Order));
  // 1 byte for type, 4 bytes for client id, rest actual order.
  return 1 + sizeof(Order);
}

void deserialise_order(const uint8_t* buffer,
                       Order& order) {  // maybe return client id?
  // Sould we assert this? I think so.
  assert(buffer[0] == static_cast<uint8_t>(MessageType::ORDER_NEW));
  Order network_order{};
  std::memcpy(&network_order, &buffer[1], sizeof(Order));
  order.order_id = be64toh(network_order.order_id);
  order.price = be64toh(network_order.price);
  order.quantity = be32toh(network_order.quantity);
  order.side = network_order.side;
  order.order_type = network_order.order_type;
  order.tif = network_order.tif;
}

auto serialise_new_login(ClientId new_id, uint8_t* buffer) -> size_t {
  buffer[0] = static_cast<uint8_t>(MessageType::LOGIN_RESPONSE);
  new_id = htobe32(new_id);
  std::memcpy(&buffer[1], &new_id, 4);
  return 5;
}

auto deserialise_new_login(const uint8_t* buffer) -> ClientId {
  assert(buffer[0] == static_cast<uint8_t>(MessageType::LOGIN_RESPONSE));
  ClientId new_id;
  std::memcpy(&new_id, &buffer[1], 4);
  new_id = be32toh(new_id);
  return new_id;
}
auto serialise_execution_report(const ExecutionReport& report, uint8_t* buffer)
    -> size_t {
  buffer[0] = static_cast<uint8_t>(MessageType::EXEC_REPORT);
  ExecutionReport network_exec_report;
  network_exec_report.order_id = htobe64(report.order_id);
  network_exec_report.price = htobe64(report.price);
  network_exec_report.client_id = htobe32(report.client_id);
  network_exec_report.last_quantity = htobe32(report.last_quantity);
  network_exec_report.remaining_quantity = htobe32(report.remaining_quantity);
  network_exec_report.side = report.side;
  network_exec_report.type = report.type;
  network_exec_report.reason = report.reason;

  std::memcpy(&buffer[1], &network_exec_report, sizeof(ExecutionReport));
  return 1 + sizeof(ExecutionReport);
}

void deserialise_execution_report(const uint8_t* buffer,
                                  ExecutionReport& exec_report) {
  assert(buffer[0] == static_cast<uint8_t>(MessageType::EXEC_REPORT));
  ExecutionReport network_exec_report{};
  std::memcpy(&network_exec_report, &buffer[1], sizeof(ExecutionReport));
  exec_report.order_id = be64toh(network_exec_report.order_id);
  exec_report.price = be64toh(network_exec_report.price);
  exec_report.client_id = be32toh(network_exec_report.client_id);
  exec_report.last_quantity = be32toh(network_exec_report.last_quantity);
  exec_report.remaining_quantity =
      be32toh(network_exec_report.remaining_quantity);
  exec_report.side = network_exec_report.side;
  exec_report.type = network_exec_report.type;
  exec_report.reason = network_exec_report.reason;
}

auto serialise_trade(const Trade& trade, uint8_t* buffer) -> size_t {
  buffer[0] = static_cast<uint8_t>(MessageType::TRADE);
  Trade network_trade;
  network_trade.price = htobe64(trade.price);
  network_trade.quantity = htobe32(trade.quantity);
  network_trade.time_stamp = htobe64(trade.time_stamp);
  network_trade.maker_order_id = htobe64(trade.maker_order_id);
  network_trade.taker_order_id = htobe64(trade.taker_order_id);
  network_trade.aggressor_side = trade.aggressor_side;

  std::memcpy(&buffer[1], &network_trade, sizeof(Trade));
  return 1 + sizeof(Trade);
}

void deserialise_trade(const uint8_t* buffer, Trade& trade) {
  assert(buffer[0] == static_cast<uint8_t>(MessageType::TRADE));
  Trade network_trade{};
  std::memcpy(&network_trade, &buffer[1], sizeof(Trade));
  trade.maker_order_id = be64toh(network_trade.maker_order_id);
  trade.price = be64toh(network_trade.price);
  trade.quantity = be32toh(network_trade.quantity);
  trade.time_stamp = be64toh(network_trade.time_stamp);
  trade.taker_order_id = be64toh(network_trade.taker_order_id);
  trade.aggressor_side = network_trade.aggressor_side;
}

// NOTE: do we really need client id here? Maybe we can do without this.
auto serialise_order_cancel(OrderId order_id, uint8_t* buffer) -> size_t {
  buffer[0] = static_cast<uint8_t>(MessageType::ORDER_CANCEL);
  order_id = htobe64(order_id);
  std::memcpy(&buffer[1], &order_id, 8);

  return 9;
}

auto deserialise_order_cancel(const uint8_t* buffer) -> OrderId {
  assert(buffer[0] == static_cast<uint8_t>(MessageType::ORDER_CANCEL));
  OrderId order_id;

  std::memcpy(&order_id, &buffer[1], 8);
  order_id = be64toh(order_id);
  return order_id;
}

// NOLINTEND
