#include "TaskManager.h"
#include <FontConfig.h>
#include <logger.h>

#define MODULE_NAME "TaskManager"

TaskManager::TaskManager() {
}

void TaskManager::addTask(Task *task) {
  _tasks.push_back(task);
}

void TaskManager::addAlwaysRunTask(Task *task) {
  _alwaysRunTasks.push_back(task);
}

void TaskManager::addFreeRTOSTask(FreeRTOSTask *task) {
  _FreeRTOSTasks.push_back(task);
}

std::list<Task *> TaskManager::getTasks() {
  std::list<Task *> tasks = _alwaysRunTasks;
  std::copy(_tasks.begin(), _tasks.end(), std::back_inserter(tasks));
  return tasks;
}

std::list<FreeRTOSTask *> TaskManager::getFreeRTOSTasks() {
  return _FreeRTOSTasks;
}

bool TaskManager::setup(System &system) {
  logger.info(MODULE_NAME, "will setup all tasks...");
  for (Task *elem : _alwaysRunTasks) {
    logger.debug(MODULE_NAME, "call setup for %s", elem->getName().c_str());
    elem->setup(system);
  }
  for (Task *elem : _tasks) {
    logger.debug(MODULE_NAME, "call setup for %s", elem->getName().c_str());
    elem->setup(system);
  }
  _nextTask = _tasks.begin();
  return true;
}

bool TaskManager::loop(System &system) {
  for (Task *elem : _alwaysRunTasks) {
    elem->loop(system);
  }

  if (_nextTask == _tasks.end()) {
    _nextTask = _tasks.begin();
  }
  bool ret = (*_nextTask)->loop(system);
  ++_nextTask;
  return ret;
}

// cppcheck-suppress unusedFunction
void StatusFrame::drawStatusPage(Bitmap &bitmap) {
  int y = 0;
  for (Task *task : _tasks) {
    int x = bitmap.drawString(0, y, (task->getName()).substring(0, task->getName().indexOf("Task")));
    x     = bitmap.drawString(x, y, ": ");
    if (task->getStateInfo() == "") {
      switch (task->getState()) {
      case Error:
        bitmap.drawString(x, y, "Error");
        break;
      case Warning:
        bitmap.drawString(x, y, "Warning");
      default:
        break;
      }
      bitmap.drawString(x, y, "Okay");
    } else {
      bitmap.drawString(x, y, task->getStateInfo());
    }
    y += getSystemFont()->heightInPixel;
  }
}

FreeRTOSTask::FreeRTOSTask(const String &name, int taskId, UBaseType_t priority, uint32_t stackDepth, BaseType_t coreId) : _state(Okay), _stateInfo("Booting"), taskStarted(false), _name(name), _taskId(taskId), _priority(priority), _stackDepth(stackDepth), _coreId(coreId) {
}

FreeRTOSTask::~FreeRTOSTask() {
#if INCLUDE_vTaskDelete
  vTaskDelete(handle);
#endif
  return;
}

void FreeRTOSTask::taskWrap(void *param) {
  fn_args *params = static_cast<fn_args *>(param);
  static_cast<FreeRTOSTask *>(params->classPtr)->worker(params->argc, params->argv);
  delete params;
#if INCLUDE_vTaskDelete
  vTaskDelete(static_cast<FreeRTOSTask *>(param)->handle);
#else
  while (1) {
    vTaskDelay(10000);
  }
#endif
}

bool FreeRTOSTask::start(int argc, void *argv) {
  if (taskStarted) {
    return false;
  } else {
    fn_args *params  = new fn_args;
    params->classPtr = this;
    params->argc     = argc;
    params->argv     = argv;
    xTaskCreateUniversal(this->taskWrap, getName().c_str(), _stackDepth, params, _priority, &handle, _coreId);
    return true;
  }
}
