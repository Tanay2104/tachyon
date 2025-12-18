#include "engine/logger.hpp"
#include "engine/constants.hpp"
#include <fstream>
#include <malloc.h>

extern std::atomic<bool> start_exchange;
extern std::atomic<bool> keep_running;

template <TachyonConfig config>
LoggerClass<config>::LoggerClass(config::EventQueue &ev_queue,
                                 config::ExecReportQueue &exec_queue,
                                 config::TradesQueue &tr_queue,
                                 config::EventQueue &prcs_events)
    : event_queue(ev_queue), execution_reports(exec_queue), trades(tr_queue),
      processed_events(prcs_events) {

  // Getting ready for later logging.
  // Writing Processed events.
  std::ofstream file;
  file.open("logs/processed_events.txt", std::ios ::out);
  file << "Processed Events by Engine\n";
  file.close();

  // Writing trades.
  file.open("logs/processed_trades.txt", std::ios::out);
  file << "Processed Trades\n";
  file.close();
}

template <TachyonConfig config> LoggerClass<config>::~LoggerClass() {
  writeProcessedEventsLogs();
  writeTradeLogs();
}

template <TachyonConfig config>
void LoggerClass<config>::logTrade(Trade &trade, ClientRequest &resting,
                                   ClientRequest &incoming,
                                   Quantity trade_quantity) {
  //  NOTE: if needed for performance, we may reconstruct trade object and not
  //  accept it as parameter.
  trades.push(trade);
  ExecutionReport exec_report{};

  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = resting.new_order.price;
  exec_report.last_quantity = trade_quantity; // Quantity traded
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::TRADE;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);

  exec_report.client_id = resting.client_id;
  exec_report.order_id = resting.new_order.order_id;
  exec_report.price = resting.new_order.price;
  exec_report.last_quantity = trade_quantity;
  exec_report.remaining_quantity = resting.new_order.quantity;
  exec_report.type = ExecType::TRADE;
  exec_report.side = resting.new_order.side;
  execution_reports.push(exec_report);
}

template <TachyonConfig config>
void LoggerClass<config>::logSelfTrade(ClientRequest &incoming) {
  ExecutionReport report{};
  report.client_id = incoming.client_id;
  report.last_quantity = 0;
  report.remaining_quantity = incoming.new_order.quantity;
  report.price = incoming.new_order.price;
  report.order_id = incoming.new_order.order_id;
  report.type = ExecType::REJECTED;
  report.reason = RejectReason::SELF_TRADE;
  report.side = incoming.new_order.side;

  execution_reports.push(report);
}

template <TachyonConfig config>
void LoggerClass<config>::logCancelOrder(ClientRequest &incoming) {
  ExecutionReport exec_report{};
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = incoming.new_order.price;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::CANCELED;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);
}

template <TachyonConfig config>
void LoggerClass<config>::logInvalidOrder(ClientRequest &incoming) {
  ExecutionReport exec_report{};
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = incoming.new_order.price;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::REJECTED;
  exec_report.reason = RejectReason::INVALID_ORDER_TYPE;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);
}

template <TachyonConfig config>
void LoggerClass<config>::logNotFound(ClientRequest &incoming) {
  ExecutionReport exec_report{};
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.order_id_to_cancel;
  exec_report.price = 0;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = 0;
  exec_report.type = ExecType::REJECTED;
  exec_report.reason = RejectReason::ORDER_NOT_FOUND;
  execution_reports.push(exec_report);
}

template <TachyonConfig config>
void LoggerClass<config>::logNewOrder(ClientRequest &incoming) {
  ExecutionReport exec_report{};
  exec_report.client_id = incoming.client_id;
  exec_report.order_id = incoming.new_order.order_id;
  exec_report.price = incoming.new_order.price;
  exec_report.last_quantity = 0;
  exec_report.remaining_quantity = incoming.new_order.quantity;
  exec_report.type = ExecType::NEW;
  exec_report.side = incoming.new_order.side;
  execution_reports.push(exec_report);
}

template <TachyonConfig config>
void LoggerClass<config>::writeProcessedEventsLogs() {
  std::ofstream processed_requests_file;
  processed_requests_file.open("logs/processed_events.txt", std::ios::app);
  ClientRequest event{};
  uint32_t before_size = processed_events.size();
  for (uint32_t i = 0; i < before_size;
       i++) { // At least MAX PROCESSED events must be there.
    processed_events.try_pop(event);
    processed_requests_file << "Client " << event.client_id << ": ";
    if (event.type == RequestType::New) {
      processed_requests_file
          << "ORDER ID " << event.new_order.order_id << " "
          << (event.new_order.side == Side::BID ? "BUY " : "SELL ")
          << event.new_order.quantity << " @ " << event.new_order.price << " "
          << (event.new_order.order_type == OrderType::LIMIT ? "LIMIT "
                                                             : "MARKET ")
          << (event.new_order.tif == TimeInForce::GTC ? "GTC " : "IOC ")
          << "TIMESTAMP-" << event.time_stamp << "\n";
    } else if (event.type == RequestType::Cancel) {
      processed_requests_file << "CANCEL " << " ORDER ID "
                              << event.order_id_to_cancel << " TIMESTAMP-"
                              << event.time_stamp << "\n";
    }
  }
  processed_requests_file.close();
}

template <TachyonConfig config>
void LoggerClass<config>::writeProcessedEventsLogsContinuous() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  while (keep_running.load(std::memory_order_relaxed)) {
    if (processed_events.size() >= MAX_PROCESSED_EVENTS_SIZE) {
      writeProcessedEventsLogs();
    }
  }
}

template <TachyonConfig config> void LoggerClass<config>::writeTradeLogs() {
  std::ofstream file;
  // Writing trades.
  file.open("logs/processed_trades.txt", std::ios::app);
  Trade trade{};
  for (uint32_t i = 0; i < MAX_TRADES_QUEUE_SIZE; i++) {
    if (trades.try_pop( // This should succeed.
            trade)) {   // Note: trades qeueue will automatically
                        // be cleared by this mechanism.
      file << "MAKER: " << trade.maker_order_id
           << " TAKER: " << trade.taker_order_id << " " << trade.quantity
           << " @ " << trade.price << " TIMESTAMP-" << trade.time_stamp << "\n";
    } else {
      std::this_thread::yield();
    }
  }

  file.close();
}

template <TachyonConfig config>
void LoggerClass<config>::writeTradeLogsContinuous() {
  while (!start_exchange.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  while (keep_running.load(std::memory_order_relaxed)) {
    if (trades.size() == MAX_TRADES_QUEUE_SIZE) {
      writeTradeLogs();
      malloc_trim(0);
    } else {
      std::this_thread::yield();
    }
  }
}

template class LoggerClass<my_config>;
