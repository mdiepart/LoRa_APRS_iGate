#ifndef TASK_MANAGER_H_
#define TASK_MANAGER_H_

#include <Arduino.h>
#include <BoardFinder.h>
#include <configuration.h>
#include <freertos/FreeRTOS.h>
#include <list>
#include <memory>

class System;

enum TaskDisplayState {
  Error,
  Warning,
  Okay,
};

class FreeRTOSTask {
public:
  FreeRTOSTask(const String &name, int taskId, UBaseType_t priority, uint32_t stackDepth = configMINIMAL_STACK_SIZE, BaseType_t coreId = -1, const bool displayOnScreen = true);
  ~FreeRTOSTask();

  xTaskHandle handle;

  virtual void worker() = 0;

  const char *getName() const {
    return _name.c_str();
  }

  int getTaskId() const {
    return _taskId;
  }

  TaskDisplayState getState() const {
    return _state;
  }

  String getStateInfo() const {
    return _stateInfo;
  }

  bool start();

  bool getDisplayOnScreen() const {
    return _displayOnScreen;
  }

protected:
  TaskDisplayState _state;
  String           _stateInfo;
  static void      taskWrap(void *param);
  bool             taskStarted;

private:
  String      _name;
  int         _taskId;
  UBaseType_t _priority;
  uint32_t    _stackDepth;
  BaseType_t  _coreId;
  bool        _displayOnScreen;
};

class TaskManager {
public:
  TaskManager();
  ~TaskManager() {
  }

  void addFreeRTOSTask(FreeRTOSTask *task);

  std::list<FreeRTOSTask *> getFreeRTOSTasks();

private:
  std::list<FreeRTOSTask *> _FreeRTOSTasks;
};

#endif
