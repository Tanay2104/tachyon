#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "engine/types.hpp"
#include "network/message.hpp"

namespace {

template <typename T>
void ExpectMemEq(const uint8_t* buffer, size_t offset, const T& value) {
  T tmp{};
  std::memcpy(&tmp, buffer + offset, sizeof(T));
  EXPECT_EQ(tmp, value);
}

template <typename T>
void WriteValue(uint8_t* buffer, size_t offset, const T& value) {
  std::memcpy(buffer + offset, &value, sizeof(T));
}

}  // namespace

TEST(OrderSerialization, RoundTripBasic) {
  Order original{
      .order_id = 123456789ULL,
      .price = 129224ULL,
      .quantity = 42,
      .side = Side::BID,
      .order_type = OrderType::LIMIT,
      .tif = TimeInForce::GTC,
  };

  uint8_t buffer[24]{};
  const size_t written = serialise_order(original, buffer);

  EXPECT_EQ(written, 24);
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::ORDER_NEW));

  Order decoded = deserialise_order(buffer);

  EXPECT_EQ(decoded.order_id, original.order_id);
  EXPECT_EQ(decoded.price, original.price);
  EXPECT_EQ(decoded.quantity, original.quantity);
  EXPECT_EQ(decoded.side, original.side);
  EXPECT_EQ(decoded.order_type, original.order_type);
  EXPECT_EQ(decoded.tif, original.tif);
}

TEST(OrderSerialization, ByteLayoutExact) {
  Order order{
      .order_id = 0x0102030405060708ULL,
      .price = 0x1112131415161718ULL,
      .quantity = 0xAABBCCDD,
      .side = Side::ASK,
      .order_type = OrderType::MARKET,
      .tif = TimeInForce::IOC,
  };

  uint8_t buffer[24]{};
  serialise_order(order, buffer);

  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::ORDER_NEW));

  ExpectMemEq(buffer, 1, order.order_id);
  ExpectMemEq(buffer, 9, order.price);
  ExpectMemEq(buffer, 17, order.quantity);
  EXPECT_EQ(buffer[21], static_cast<uint8_t>(order.side));
  EXPECT_EQ(buffer[22], static_cast<uint8_t>(order.order_type));
  EXPECT_EQ(buffer[23], static_cast<uint8_t>(order.tif));
}

TEST(OrderSerialization, DeathOnWrongMessageType) {
  uint8_t buffer[24]{};
  buffer[0] = static_cast<uint8_t>(MessageType::TRADE);

  ASSERT_DEATH(
      {
        auto o = deserialise_order(buffer);
        (void)o;
      },
      ".*");
}

TEST(ExecutionReportSerialization, RoundTrip) {
  ExecutionReport original{
      .client_id = 42,
      .order_id = 999999ULL,
      .price = 1234500ULL,
      .last_quantity = 10,
      .remaining_quantity = 90,
      .type = ExecType::TRADE,
      .reason = RejectReason::NONE,
      .side = Side::ASK,
  };

  uint8_t buffer[32]{};
  const size_t written = serialise_execution_report(original, buffer);

  EXPECT_EQ(written, 32);
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::EXEC_REPORT));

  ExecutionReport decoded = deserialise_execution_report(buffer);

  EXPECT_EQ(decoded.client_id, original.client_id);
  EXPECT_EQ(decoded.order_id, original.order_id);
  EXPECT_EQ(decoded.price, original.price);
  EXPECT_EQ(decoded.last_quantity, original.last_quantity);
  EXPECT_EQ(decoded.remaining_quantity, original.remaining_quantity);
  EXPECT_EQ(decoded.type, original.type);
  EXPECT_EQ(decoded.reason, original.reason);
  EXPECT_EQ(decoded.side, original.side);
}

TEST(ExecutionReportSerialization, ByteOffsetsCorrect) {
  ExecutionReport r{
      .client_id = 0x11223344,
      .order_id = 0x0102030405060708ULL,
      .price = 0x1112131415161718ULL,
      .last_quantity = 0xAABBCCDD,
      .remaining_quantity = 0xEEFF0011,
      .type = ExecType::REJECTED,
      .reason = RejectReason::PRICE_INVALID,
      .side = Side::BID,
  };

  uint8_t buffer[32]{};
  serialise_execution_report(r, buffer);

  ExpectMemEq(buffer, 1, r.client_id);
  ExpectMemEq(buffer, 5, r.order_id);
  ExpectMemEq(buffer, 13, r.price);
  ExpectMemEq(buffer, 21, r.last_quantity);
  ExpectMemEq(buffer, 25, r.remaining_quantity);
  EXPECT_EQ(buffer[29], static_cast<uint8_t>(r.type));
  EXPECT_EQ(buffer[30], static_cast<uint8_t>(r.reason));
  EXPECT_EQ(buffer[31], static_cast<uint8_t>(r.side));
}

TEST(TradeSerialization, RoundTrip) {
  Trade original{
      .maker_order_id = 1,
      .taker_order_id = 2,
      .time_stamp = 123456789ULL,
      .price = 999999ULL,
      .quantity = 77,
      .aggressor_side = Side::ASK,
  };

  uint8_t buffer[38]{};
  const size_t written = serialise_trade(original, buffer);

  EXPECT_EQ(written, 38);
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::TRADE));

  Trade decoded = deserialise_trade(buffer);

  EXPECT_EQ(decoded.maker_order_id, original.maker_order_id);
  EXPECT_EQ(decoded.taker_order_id, original.taker_order_id);
  EXPECT_EQ(decoded.time_stamp, original.time_stamp);
  EXPECT_EQ(decoded.price, original.price);
  EXPECT_EQ(decoded.quantity, original.quantity);
  EXPECT_EQ(decoded.aggressor_side, original.aggressor_side);
}

TEST(OrderCancelSerialization, RoundTrip) {
  ClientId client_id = 1234;
  OrderId order_id = 0x0102030405060708ULL;

  uint8_t buffer[13]{};
  const size_t written = serialize_order_cancel(client_id, order_id, buffer);

  EXPECT_EQ(written, 13);
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::ORDER_CANCEL));

  auto [decoded_client, decoded_order] = deserialize_order_cancel(buffer);

  EXPECT_EQ(decoded_client, client_id);
  EXPECT_EQ(decoded_order, order_id);
}

TEST(SerializationEdgeCases, MaxValues) {
  Order order{
      .order_id = UINT64_MAX,
      .price = UINT64_MAX,
      .quantity = UINT32_MAX,
      .side = Side::ASK,
      .order_type = OrderType::MARKET,
      .tif = TimeInForce::IOC,
  };

  uint8_t buffer[24]{};
  serialise_order(order, buffer);

  Order decoded = deserialise_order(buffer);
  EXPECT_EQ(decoded.order_id, UINT64_MAX);
  EXPECT_EQ(decoded.price, UINT64_MAX);
  EXPECT_EQ(decoded.quantity, UINT32_MAX);
}
