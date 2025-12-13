#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "engine/orderbook.hpp"
#include "engine/types.hpp"
#include <engine/constants.hpp>
// ============================================================================
// Test Helper: Deterministic Request Generator
// ============================================================================
class OrderBookTest : public ::testing::Test {
 protected:
  OrderBook book;
  std::vector<std::pair<Trade, ClientRequest>> trades;
  TimeStamp current_time = 1000;

  void SetUp() override {
    // Reset state before each test
    trades.clear();
    current_time = 1000;
  }

  ClientRequest makeReq(ClientId cid, OrderId oid, Side side, Price price,
                        Quantity qty, RequestType type = RequestType::New) {
    ClientRequest req;
    req.type = type;
    req.client_id = cid;
    req.time_stamp =
        current_time++;  // Auto-increment for strict time priority testing

    if (type == RequestType::New) {
      req.new_order.order_id = oid;
      req.new_order.side = side;
      req.new_order.price = CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN + price;
      req.new_order.quantity = qty;
      req.new_order.order_type = OrderType::LIMIT;
      req.new_order.tif = TimeInForce::GTC;
    } else {
      req.order_id_to_cancel = oid;
    }
    return req;
  }
};

// ============================================================================
// 1. Basic Matching Logic
// ============================================================================

TEST_F(OrderBookTest, SingleOrderNoMatch) {
  // Place a Sell order
  auto sell = makeReq(1, 101, Side::ASK, 100, 10);
  book.add(sell);

 //  EXPECT_EQ(book.size_asks(), 1);
  // Place a Buy order with price too low
  auto buyLow = makeReq(2, 201, Side::BID, 90, 10);  // Buy @ 90 vs Sell @ 100
  book.match(buyLow, trades);

  EXPECT_TRUE(trades.empty());
  // Both orders should be resting now (Ask @ 100, Bid @ 90)
}

TEST_F(OrderBookTest, FullMatch) {
  // 1. Resting Sell @ 100
  auto sell = makeReq(1, 101, Side::ASK, 100, 50);
  book.add(sell);

  // 2. Aggressive Buy @ 100
  auto buy = makeReq(2, 201, Side::BID, 100, 50);
  book.match(buy, trades);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.quantity, 50);
  EXPECT_EQ(trades[0].first.price, CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN + 100);
  EXPECT_EQ(trades[0].first.maker_order_id, 101);
  EXPECT_EQ(trades[0].first.taker_order_id, 201);
}

TEST_F(OrderBookTest, AggressorPriceImprovement) {
  // 1. Resting Sell @ 100
  auto sell = makeReq(1, 101, Side::ASK, 100, 10);
  book.add(sell);

  // 2. Aggressive Buy @ 110 (Willing to pay more)
  // Should match at the resting price (100), not aggressive price (110)
  auto buy = makeReq(2, 201, Side::BID, 110, 10);
  book.match(buy, trades);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.price, CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN + 100);
  EXPECT_EQ(trades[0].first.quantity, 10);
}

// ============================================================================
// 2. Priority Logic (Price & Time)
// ============================================================================

TEST_F(OrderBookTest, PricePriority_Bid) {
  // Resting Bids: 100, 101, 102
  auto b1 = makeReq(1, 100, Side::BID, 100, 10);
  book.add(b1);
  auto b2 = makeReq(2, 101, Side::BID, 101, 10);
  book.add(b2);
  auto b3 = makeReq(3, 102, Side::BID, 102, 10);
  book.add(b3);

  // Aggressive Sell @ 99 (sweeps all)
  // Should match 102 first, then 101, then 100
  auto sell = makeReq(4, 200, Side::ASK, 99, 30);
  book.match(sell, trades);

  ASSERT_EQ(trades.size(), 3);
  EXPECT_EQ(trades[0].first.maker_order_id, 102);  // Highest Price
  EXPECT_EQ(trades[1].first.maker_order_id, 101);
  EXPECT_EQ(trades[2].first.maker_order_id, 100);  // Lowest Price
}

TEST_F(OrderBookTest, PricePriority_Ask) {
  // Resting Asks: 100, 101, 102
  auto a1 = makeReq(1, 100, Side::ASK, 100, 10);
  book.add(a1);
  auto a2 = makeReq(2, 101, Side::ASK, 101, 10);
  book.add(a2);
  auto a3 = makeReq(3, 102, Side::ASK, 102, 10);
  book.add(a3);

  // Aggressive Buy @ 105
  // Should match 100 first (Best Deal), then 101, then 102
  auto buy = makeReq(4, 200, Side::BID, 105, 30);
  book.match(buy, trades);

  ASSERT_EQ(trades.size(), 3);
  EXPECT_EQ(trades[0].first.maker_order_id, 100);  // Lowest Price
  EXPECT_EQ(trades[1].first.maker_order_id, 101);
  EXPECT_EQ(trades[2].first.maker_order_id, 102);
}

TEST_F(OrderBookTest, TimePriority_Simple) {
  // Two orders at SAME price, different times
  auto s1 = makeReq(1, 101, Side::ASK, 100, 10);  // Time 1000
  book.add(s1);

  auto s2 = makeReq(2, 102, Side::ASK, 100, 10);  // Time 1001
  book.add(s2);

  // Buy 10 units
  auto buy = makeReq(3, 200, Side::BID, 100, 10);
  book.match(buy, trades);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.maker_order_id, 101);  // Earlier timestamp matched
}

// ============================================================================
// 3. Partial Fills & Queue Position
// ============================================================================

TEST_F(OrderBookTest, PartialFill_AggressorRemains) {
  // Sell 10 @ 100
  auto sell = makeReq(1, 101, Side::ASK, 100, 10);
  book.add(sell);

  // Buy 15 @ 100 (Aggressor is larger)
  auto buy = makeReq(2, 201, Side::BID, 100, 15);
  book.match(buy, trades);

  // 1. Check Trade
  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.quantity, 10);

  // 2. Check that the remaining 5 Buy units are now resting
  // We verify this by sending a new Sell order to match the remainder
  // NOTE: we try to simulate the result as the Engine handles putting orders
  // back into orderbook for clear separation.
  if (buy.new_order.quantity > 0) {
    book.add(buy);
  }

  trades.clear();
  auto sell2 = makeReq(3, 102, Side::ASK, 100, 5);
  book.match(sell2, trades);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.maker_order_id,
            201);  // The previous aggressor became the maker
  EXPECT_EQ(trades[0].first.quantity, 5);
}

TEST_F(OrderBookTest, PartialFill_RestingOrderRetainsPriority) {
  // This is a CRITICAL test for correct Order Book behavior.
  // If an order is partially filled, it must NOT lose its place in the queue
  // (Time Priority).

  // 1. Resting Order A: Sell 100 @ 100 (Time 1000)
  auto sellA = makeReq(1, 101, Side::ASK, 100, 100);
  book.add(sellA);

  // 2. Resting Order B: Sell 50 @ 100 (Time 1001)
  auto sellB = makeReq(2, 102, Side::ASK, 100, 50);
  book.add(sellB);

  // 3. Aggressive Buy 50 @ 100. Should partially fill A.
  auto buy1 = makeReq(3, 201, Side::BID, 100, 50);
  book.match(buy1, trades);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.maker_order_id, 101);  // Matched A
  trades.clear();

  // 4. Aggressive Buy 60 @ 100.
  // Should match remaining 50 of A (because it still has priority over B), then
  // 10 of B.
  auto buy2 = makeReq(4, 202, Side::BID, 100, 60);
  book.match(buy2, trades);

  ASSERT_EQ(trades.size(), 2);

  // First trade matches remaining A
  EXPECT_EQ(trades[0].first.maker_order_id, 101);
  EXPECT_EQ(trades[0].first.quantity, 50);

  // Second trade matches start of B
  EXPECT_EQ(trades[1].first.maker_order_id, 102);
  EXPECT_EQ(trades[1].first.quantity, 10);
}

// ============================================================================
// 4. Complex Logic: Walking the Book
// ============================================================================

TEST_F(OrderBookTest, WalkingTheBook) {
  // Setup a ladder of asks
  book.add(*new ClientRequest(makeReq(1, 10, Side::ASK, 100, 10)));
  book.add(*new ClientRequest(makeReq(1, 11, Side::ASK, 101, 10)));
  book.add(*new ClientRequest(makeReq(1, 12, Side::ASK, 102, 10)));

  // Aggressive Large Buy @ 105
  auto buy = makeReq(2, 99, Side::BID, 105, 25);
  book.match(buy, trades);

  ASSERT_EQ(trades.size(), 3);

  // 1. Clears 10 @ 100
  EXPECT_EQ(trades[0].first.maker_order_id, 10);
  EXPECT_EQ(trades[0].first.price, CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN + 100);

  // 2. Clears 10 @ 101
  EXPECT_EQ(trades[1].first.maker_order_id, 11);
  EXPECT_EQ(trades[1].first.price, CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN + 101);

  // 3. Clears 5 @ 102 (Partial)
  EXPECT_EQ(trades[2].first.maker_order_id, 12);
  EXPECT_EQ(trades[2].first.price, CLIENT_BASE_PRICE + CLIENT_PRICE_DISTRIB_MIN +102);
  EXPECT_EQ(trades[2].first.quantity, 5);
}

// ============================================================================
// 5. Edge Case: Self-Trade Prevention
// ============================================================================

TEST_F(OrderBookTest, SelfTradePrevention) {
  // Client 1 places Sell Order
  auto sell = makeReq(1, 100, Side::ASK, 100, 50);
  book.add(sell);

  // Client 1 places Buy Order (Same ID)
  auto buy = makeReq(1, 200, Side::BID, 100, 50);
  book.match(buy, trades);

  // Should NOT trade.
  // Based on code reading: implementation skips matching but continues
  // iterating. The aggressor remains in the book (or logic might vary, but
  // trade count must be 0).
  EXPECT_EQ(trades.size(), 0);
}

TEST_F(OrderBookTest, SelfTradeSkip) {
  // Scenario:
  // Order A (Client 1) @ 100
  // Order B (Client 2) @ 100
  // Aggressor (Client 1) Buys @ 100.
  // Should skip A (Self-trade) and match B.

  auto sellA = makeReq(1, 101, Side::ASK, 100, 10);
  book.add(sellA);

  auto sellB = makeReq(2, 102, Side::ASK, 100, 10);
  book.add(sellB);

  auto buy = makeReq(1, 201, Side::BID, 100, 20);
  book.match(buy, trades);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].first.maker_order_id, 102);  // Matched Client 2
  EXPECT_EQ(trades[0].first.quantity, 10);

  // Note: The remaining 10 quantity of the aggressor might be resting or
  // rejected depending on specific STP implementation, but the match
  // verification is key here.
}

// ============================================================================
// 6. Management: Cancellations
// ============================================================================

TEST_F(OrderBookTest, CancelExistingOrder) {
  auto sell = makeReq(1, 101, Side::ASK, 100, 10);
  book.add(sell);

  ClientRequest cancelled;
  bool success = book.cancelOrder(101, cancelled);

  EXPECT_TRUE(success);
  EXPECT_EQ(cancelled.new_order.order_id, 101);

  // Verify it's gone by trying to match against it
  auto buy = makeReq(2, 201, Side::BID, 100, 10);
  book.match(buy, trades);
  EXPECT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, CancelNonExistentOrder) {
  ClientRequest cancelled;
  bool success = book.cancelOrder(9999, cancelled);  // Random ID
  EXPECT_FALSE(success);
}

TEST_F(OrderBookTest, CancelBidVsAsk) {
  // Add Bid
  auto bid = makeReq(1, 500, Side::BID, 100, 10);
  book.add(bid);

  // Add Ask
  auto ask = makeReq(1, 501, Side::ASK, 110, 10);
  book.add(ask);

  ClientRequest out;
  // Cancel the Bid
  EXPECT_TRUE(book.cancelOrder(500, out));
  EXPECT_EQ(out.new_order.side, Side::BID);

  // Cancel the Ask
  EXPECT_TRUE(book.cancelOrder(501, out));
  EXPECT_EQ(out.new_order.side, Side::ASK);
}
