#ifndef TASK_MANAGER_H_
#define TASK_MANAGER_H_

#include <Arduino.h>
#include <BoardFinder.h>
#include <Display.h>
#include <configuration.h>
#include <freertos/FreeRTOS.h>
#include <list>
#include <memory>

#include "TaskQueue.h"

class System;

enum TaskDisplayState {
  Error,
  Warning,
  Okay,
};

class Task {
public:
  Task(String &name, int taskId) : _state(Okay), _stateInfo("Booting"), _name(name), _taskId(taskId) {
  }
  Task(const char *name, int taskId) : _state(Okay), _stateInfo("Booting"), _name(name), _taskId(taskId) {
  }
  virtual ~Task() {
  }

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

  virtual bool setup(System &system) = 0;
  virtual bool loop(System &system)  = 0;

protected:
  TaskDisplayState _state;
  String           _stateInfo;

private:
  String _name;
  int    _taskId;
};

class FreeRTOSTask {
public:
  FreeRTOSTask(const String &name, int taskId, UBaseType_t priority, uint32_t stackDepth = configMINIMAL_STACK_SIZE, BaseType_t coreId = -1);
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
};

class TaskManager {
public:
  TaskManager();
  ~TaskManager() {
  }

  void addTask(Task *task);
  void addAlwaysRunTask(Task *task);
  void addFreeRTOSTask(FreeRTOSTask *task);

  std::list<Task *>         getTasks();
  std::list<FreeRTOSTask *> getFreeRTOSTasks();

  bool setup(System &system);
  bool loop(System &system);

private:
  std::list<Task *>           _tasks;
  std::list<Task *>::iterator _nextTask;
  std::list<Task *>           _alwaysRunTasks;
  std::list<FreeRTOSTask *>   _FreeRTOSTasks;
};

class StatusFrame : public DisplayFrame {
public:
  explicit StatusFrame(const std::list<Task *> &tasks) : _tasks(tasks) {
  }
  virtual ~StatusFrame() {
  }
  void drawStatusPage(Bitmap &bitmap) override;

private:
  std::list<Task *> _tasks;
};

#include "System.h"

#endif
