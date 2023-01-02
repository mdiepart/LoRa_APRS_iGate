
#include "System.h"
#include "../../src/TaskPacketLogger.h"

System::System() : _boardConfig(0), _userConfig(0), _isEthConnected(false), _isWifiConnected(false) {
  _packetLogger = NULL;
}

System::~System() {
}

void System::setBoardConfig(BoardConfig const *const boardConfig) {
  _boardConfig = boardConfig;
}

void System::setUserConfig(Configuration const *const userConfig) {
  _userConfig = userConfig;
}

BoardConfig const *const System::getBoardConfig() const {
  return _boardConfig;
}

Configuration const *const System::getUserConfig() const {
  return _userConfig;
}

TaskManager &System::getTaskManager() {
  return _taskManager;
}

Display &System::getDisplay() {
  return _display;
}

bool System::isWifiOrEthConnected() const {
  return _isEthConnected || _isWifiConnected;
}

void System::connectedViaEth(bool status) {
  _isEthConnected = status;
}

void System::connectedViaWifi(bool status) {
  _isWifiConnected = status;
}

logging::Logger &System::getLogger() {
  return _logger;
}

void System::setPacketLogger(PacketLoggerTask *task) {
  _packetLogger = task;
}

PacketLoggerTask *System::getPacketLogger() {
  return _packetLogger;
}

void System::log_debug(String name, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  getLogger().vlogf(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, name, fmt, args);
  va_end(args);
}

void System::log_info(String name, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  getLogger().vlogf(logging::LoggerLevel::LOGGER_LEVEL_INFO, name, fmt, args);
  va_end(args);
}

void System::log_error(String name, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  getLogger().vlogf(logging::LoggerLevel::LOGGER_LEVEL_ERROR, name, fmt, args);
  va_end(args);
}

void System::log_warn(String name, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  getLogger().vlogf(logging::LoggerLevel::LOGGER_LEVEL_WARN, name, fmt, args);
  va_end(args);
}
