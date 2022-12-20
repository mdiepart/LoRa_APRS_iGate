#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <logger.h>
#include <memory>

#include "TaskManager.h"
#include <BoardFinder.h>
#include <Display.h>
#include <configuration.h>

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
  logging::Logger           &getLogger();

  void __attribute__((format(printf, 3, 4))) log_info(String name, const char *fmt, ...);
  void __attribute__((format(printf, 3, 4))) log_debug(String name, const char *fmt, ...);
  void __attribute__((format(printf, 3, 4))) log_error(String name, const char *fmt, ...);
  void __attribute__((format(printf, 3, 4))) log_warn(String name, const char *fmt, ...);

private:
  BoardConfig const   *_boardConfig;
  Configuration const *_userConfig;
  TaskManager          _taskManager;
  Display              _display;
  bool                 _isEthConnected;
  bool                 _isWifiConnected;
  logging::Logger      _logger;
};

#endif
