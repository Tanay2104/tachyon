#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <vector>

// Include your header
#include "engine/types.hpp"
#include "network/serialise.hpp"

// ============================================================================
// 1. ORDER SERIALIZATION TESTS
// ============================================================================

TEST(SerializationTest, Order_RoundTrip_Basic) {
  // 1. Setup Input
  Order original;
  original.order_id = 123456789;
  original.price = 10050;  // 100.50
  original.quantity = 500;
  original.side = Side::BID;
  original.order_type = OrderType::LIMIT;
  original.tif = TimeInForce::GTC;

  // 2. Serialize
  uint8_t buffer[128];                     // Plenty of space
  std::memset(buffer, 0, sizeof(buffer));  // Zero out for safety
  size_t bytes_written = serialise_order(original, buffer);

  // 3. Verify Size
  // Header (1) + Struct Size
  ASSERT_EQ(bytes_written, 1 + sizeof(Order));
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::ORDER_NEW));

  // 4. Deserialize
  Order result;
  deserialise_order(buffer, result);

  // 5. Verify Logic
  EXPECT_EQ(result.order_id, original.order_id);
  EXPECT_EQ(result.price, original.price);
  EXPECT_EQ(result.quantity, original.quantity);
  EXPECT_EQ(result.side, original.side);
  EXPECT_EQ(result.order_type, original.order_type);
  EXPECT_EQ(result.tif, original.tif);
}

TEST(SerializationTest, Order_RawBytes_EndiannessCheck) {
  /*
     RIGOROUS TEST:
     We verify that the bytes in the buffer are actually Big Endian.
     If we are on x86 (Little Endian), the bytes should be reversed in the
     buffer.
  */

  Order original;
  // 0x0102030405060708
  original.order_id = 0x0102030405060708ULL;
  // 0xAABBCCDD
  original.price = 0xAABBCCDDULL;
  // 0xEEFF
  original.quantity = 0x11223344;
  original.side = Side::ASK;

  uint8_t buffer[128];
  serialise_order(original, buffer);

  // Byte 0: Message Type
  ASSERT_EQ(buffer[0], static_cast<uint8_t>(MessageType::ORDER_NEW));

  // Bytes 1-8: Order ID (uint64_t) -> Should be Big Endian (01 02 ... 08)
  // On Little Endian Machine, raw memory is 08 07...
  // The serializer should have flipped it to 01 02...
  EXPECT_EQ(buffer[1], 0x01);
  EXPECT_EQ(buffer[8], 0x08);

  // Bytes 9-16: Price (uint64_t)
  // Note: Padding might shift indices depending on struct layout.
  // We assume packed or standard alignment.
  // Let's inspect via offsetof to be safe or assuming your struct logic:
  // Your serializer uses memcpy(&buffer[1], &network_struct).
  // So we check the network_struct layout logic.

  // Manual inspection pointer
  const uint8_t* ptr = &buffer[1];

  // Check OrderID at offset 0 of struct
  EXPECT_EQ(ptr[0], 0x01);

  // Check Price at offset 8 (sizeof OrderId)
  // 0xAABBCCDD -> Network: AA BB CC DD (padded to 8 bytes if uint64)
  // Since Price is uint64: 00 00 00 00 AA BB CC DD
  EXPECT_EQ(ptr[8], 0x00);
  EXPECT_EQ(ptr[15], 0xDD);
}

TEST(SerializationTest, Order_BoundaryValues) {
  Order original;
  original.order_id = std::numeric_limits<OrderId>::max();  // UINT64_MAX
  original.price = std::numeric_limits<Price>::max();
  original.quantity = std::numeric_limits<Quantity>::max();

  uint8_t buffer[128];
  serialise_order(original, buffer);

  Order result;
  deserialise_order(buffer, result);

  EXPECT_EQ(result.order_id, std::numeric_limits<OrderId>::max());
  EXPECT_EQ(result.price, std::numeric_limits<Price>::max());
  EXPECT_EQ(result.quantity, std::numeric_limits<Quantity>::max());
}

// ============================================================================
// 2. EXECUTION REPORT TESTS
// ============================================================================

TEST(SerializationTest, ExecReport_RoundTrip) {
  ExecutionReport original;
  original.client_id = 99;
  original.order_id = 888;
  original.price = 12345;
  original.last_quantity = 10;
  original.remaining_quantity = 90;
  original.type = ExecType::TRADE;
  original.reason = RejectReason::NONE;
  original.side = Side::BID;

  uint8_t buffer[128];
  size_t bytes = serialise_execution_report(original, buffer);

  ASSERT_EQ(bytes, 1 + sizeof(ExecutionReport));
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::EXEC_REPORT));

  ExecutionReport result;
  deserialise_execution_report(buffer, result);

  EXPECT_EQ(result.client_id, original.client_id);
  EXPECT_EQ(result.order_id, original.order_id);
  EXPECT_EQ(result.type, original.type);
  EXPECT_EQ(result.reason, original.reason);
}

// ============================================================================
// 3. TRADE TESTS
// ============================================================================

TEST(SerializationTest, Trade_RoundTrip) {
  Trade original;
  original.maker_order_id = 1001;
  original.taker_order_id = 2002;
  original.price = 50000;
  original.quantity = 150;
  original.time_stamp = 123456789000;
  original.aggressor_side = Side::ASK;

  uint8_t buffer[128];
  serialise_trade(original, buffer);

  Trade result;
  deserialise_trade(buffer, result);

  EXPECT_EQ(result.maker_order_id, original.maker_order_id);
  EXPECT_EQ(result.taker_order_id, original.taker_order_id);
  EXPECT_EQ(result.price, original.price);
  EXPECT_EQ(result.time_stamp, original.time_stamp);
  EXPECT_EQ(result.aggressor_side, original.aggressor_side);
}

// ============================================================================
// 4. CANCEL ORDER TESTS (Manual Memcpy Check)
// ============================================================================

TEST(SerializationTest, Cancel_RoundTrip_ManualPacking) {
  ClientId cid = 500;
  OrderId oid = 999999;

  uint8_t buffer[128];
  size_t len = serialise_order_cancel(cid, oid, buffer);

  // Verify Size (1 byte type + 4 byte cid + 8 byte oid)
  ASSERT_EQ(len, 13);
  EXPECT_EQ(buffer[0], static_cast<uint8_t>(MessageType::ORDER_CANCEL));

  auto [res_cid, res_oid] = deserialise_order_cancel(buffer);

  EXPECT_EQ(res_cid, cid);
  EXPECT_EQ(res_oid, oid);
}

TEST(SerializationTest, Cancel_ByteAlignment) {
  ClientId cid = 0x12345678;         // 4 bytes
  OrderId oid = 0xAABBCCDDEEFF0011;  // 8 bytes

  uint8_t buffer[128];
  serialise_order_cancel(cid, oid, buffer);

  // Check Client ID (Big Endian) at offset 1
  // 0x12 0x34 0x56 0x78
  EXPECT_EQ(buffer[1], 0x12);
  EXPECT_EQ(buffer[4], 0x78);

  // Check Order ID (Big Endian) at offset 5
  // 0xAA ... 0x11
  EXPECT_EQ(buffer[5], 0xAA);
  EXPECT_EQ(buffer[12], 0x11);
}
