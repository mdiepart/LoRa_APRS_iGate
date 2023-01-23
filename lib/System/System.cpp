#include "System.h"
#include "../../src/TaskPacketLogger.h"

System::System() : _boardConfig(0), _userConfig(0), _taskManager(), _isEthConnected(false), _isWifiConnected(false), _packetLogger(NULL) {
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

bool System::isWifiOrEthConnected() const {
  return _isEthConnected || _isWifiConnected;
}

void System::connectedViaEth(bool status) {
  _isEthConnected = status;
}

void System::connectedViaWifi(bool status) {
  _isWifiConnected = status;
}

void System::setPacketLogger(PacketLoggerTask *task) {
  _packetLogger = task;
}

PacketLoggerTask *System::getPacketLogger() {
  return _packetLogger;
}
