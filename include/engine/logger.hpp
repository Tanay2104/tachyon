#pragma once
#include <engine/concepts.hpp>
#include <engine/types.hpp>

template <TachyonConfig config> class LoggerClass {
private:
  config::EventQueue &event_queue;
  config::ExecReportQueue &execution_reports;
  config::TradesQueue &trades;
  config::EventQueue &processed_events;

public:
  LoggerClass(config::EventQueue &ev_queue, config::ExecReportQueue &exec_queue,
              config::TradesQueue &tr_queue, config::EventQueue &prcs_events);
  ~LoggerClass();
  void logNotFound(ClientRequest &incoming);
  void logSelfTrade(ClientRequest &incoming); // User tried self trade
  void logInvalidOrder(ClientRequest &incoming);
  void logNewOrder(ClientRequest &incoming);    // New order logged
  void logCancelOrder(ClientRequest &incoming); // Cancellation successful.
  void
  logTrade(Trade &trade, ClientRequest &resting, ClientRequest &incoming,
           Quantity trade_quantity); // Trade has happened, send report to both.

  void writeProcessedEventsLogs();
  void writeProcessedEventsLogsContinuous();
  void writeTradeLogs();
  void writeTradeLogsContinuous();
};
