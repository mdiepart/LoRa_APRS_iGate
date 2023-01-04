#ifndef SYSTEM_H_
#define SYSTEM_H_

#include "../../src/TaskPacketLogger.h"
#include "TaskManager.h"

#include <BoardFinder.h>
#include <Display.h>
#include <configuration.h>
#include <logger.h>
#include <memory>

class PacketLoggerTask;

class System {
public:
  System();
  ~System();

  void setBoardConfig(BoardConfig const *const boardConfig);
  void setUserConfig(Configuration const *const userConfig);

  BoardConfig const *const   getBoardConfig() const;
  Configuration const *const getUserConfig() const;
  TaskManager               &getTaskManager();
  Display                   &getDisplay();
  bool                       isWifiOrEthConnected() const;
  void                       connectedViaEth(bool status);
  void                       connectedViaWifi(bool status);
  void                       setPacketLogger(PacketLoggerTask *task);
  PacketLoggerTask          *getPacketLogger();

private:
  BoardConfig const   *_boardConfig;
  Configuration const *_userConfig;
  TaskManager          _taskManager;
  Display              _display;
  bool                 _isEthConnected;
  bool                 _isWifiConnected;
  PacketLoggerTask    *_packetLogger;
};

#endif
