#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <Arduino.h>
#include <WiFiUdp.h>

#include "logger_level.h"

#define APP_LOGI(module, fmt, ...) log_printf(LOG_COLOR_I "[INFO][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)
#define APP_LOGD(module, fmt, ...) log_printf(LOG_COLOR_D "[DEBUG][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)
#define APP_LOGE(module, fmt, ...) log_printf(LOG_COLOR_E "[ERROR][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)
#define APP_LOGW(module, fmt, ...) log_printf(LOG_COLOR_W "[WARN][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)

#define APP_ISR_LOGI(module, fmt, ...) ets_printf(LOG_COLOR_I "[INFO][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)
#define APP_ISR_LOGD(module, fmt, ...) ets_printf(LOG_COLOR_D "[DEBUG][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)
#define APP_ISR_LOGE(module, fmt, ...) ets_printf(LOG_COLOR_E "[ERROR][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)
#define APP_ISR_LOGW(module, fmt, ...) ets_printf(LOG_COLOR_W "[WARN][%s] " fmt LOG_RESET_COLOR "\r\n", module, ##__VA_ARGS__)

namespace logging {

class Logger {
public:
  Logger();
  Logger(LoggerLevel level);
  Logger(Stream *serial);
  Logger(Stream *serial, LoggerLevel level);
  ~Logger();

  void setSerial(Stream *serial);
  void setDebugLevel(LoggerLevel level);

  // syslog config
  void setSyslogServer(const String &server, unsigned int port, const String &hostname);
  void setSyslogServer(IPAddress ip, unsigned int port, const String &hostname);

  // Serial logging
  void __attribute__((format(printf, 3, 4))) info(String name, const char *fmt, ...);
  void __attribute__((format(printf, 3, 4))) debug(String name, const char *fmt, ...);
  void __attribute__((format(printf, 3, 4))) error(String name, const char *fmt, ...);
  void __attribute__((format(printf, 3, 4))) warn(String name, const char *fmt, ...);

  void __attribute__((format(printf, 4, 5))) log(LoggerLevel level, const String &module, const char *fmt, ...);
  void vlogf(LoggerLevel level, const String &module, const char *fmt, va_list args);

private:
  Stream     *_serial;
  LoggerLevel _level;

  void println(LoggerLevel level, const String &module, const String &text);
  void printHeader(LoggerLevel level, const String &module);

  // syslog members
  bool      _isSyslogSet;
  WiFiUDP   _syslogUdp;
  String    _syslogServer;
  IPAddress _syslogIp;
  int       _syslogPort;
  String    _syslogHostname;

  void syslogLog(LoggerLevel level, const String &module, const String &text);
};
} // namespace logging

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_LOGGER)
extern logging::Logger logger;
#endif

#endif
