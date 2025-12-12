#include "containers/lock_queue.hpp"
#include "engine/engine.hpp"
#include "engine/orderbook.hpp"
#include "engine/types.hpp"
#include "gtest/gtest.h"

std::atomic<bool> keep_running(true);
std::atomic<bool> start_exchange(false);
// Helper to make order creation less painful.
ClientRequest createLimitOrder(ClientId cid, OrderId oid, Side side,
                               Price price, Quantity qty) {
  ClientRequest req;
  req.type = RequestType::New;
  req.client_id = cid;
  req.time_stamp = 1000;
  req.new_order.order_id = oid;
  req.new_order.price = price;
  req.new_order.quantity = qty;
  req.new_order.side = side;
  req.new_order.order_type = OrderType::LIMIT;
  req.new_order.tif = TimeInForce::GTC;
  return req;
}

class EngineTest : public ::testing::Test {
 protected:
  threadsafe::stl_queue<ClientRequest> event_q;
  threadsafe::stl_queue<Trade> trade_q;
  threadsafe::stl_queue<ExecutionReport> report_q;
  OrderBook book;

  Engine engine;
  EngineTest() : engine(event_q, trade_q, report_q, book) {
    start_exchange.store(true, std::memory_order_release);
  }
};
// Task 1: Simple aggressive buy.
TEST_F(EngineTest, AggressiveBuyMatchesRestingSell) {
  // Add resting sell order.
  ClientRequest sell = createLimitOrder(1, 101, Side::ASK, 10000, 100);
  engine.handle_ASK_GTC_LIMIT(sell);  // Directly called so synchronous.
  ASSERT_EQ(book.asks.size(), 1);
  ASSERT_EQ(book.bids.size(), 0);

  // Add aggressive buy order.
  ClientRequest buy = createLimitOrder(2, 201, Side::BID, 10000, 50);
  engine.handle_BID_GTC_LIMIT(buy);

  // Verify trade logic.
  ASSERT_FALSE(trade_q.empty());

  Trade t;
  trade_q.try_pop(t);

  EXPECT_EQ(t.maker_order_id, 101);
  EXPECT_EQ(t.taker_order_id, 201);
  EXPECT_EQ(t.price, 10000);
  EXPECT_EQ(t.quantity, 50);

  // Verify book state
  ASSERT_EQ(book.asks.size(), 1);
}

// Task 2: Price improvement.
TEST_F(EngineTest, PriceImprovement) {
  // Maker: Sell @ 90
  engine.handle_ASK_GTC_LIMIT(createLimitOrder(1, 101, Side::ASK, 9000, 100));
  engine.handle_BID_GTC_LIMIT(createLimitOrder(2, 201, Side::BID, 10000, 100));

  Trade t;
  trade_q.try_pop(t);

  EXPECT_EQ(t.price, 9000);
}
