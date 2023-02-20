#include "TaskManager.h"
#include <FontConfig.h>
#include <logger.h>

#define MODULE_NAME "TaskManager"

TaskManager::TaskManager() {
}

void TaskManager::addFreeRTOSTask(FreeRTOSTask *task) {
  _FreeRTOSTasks.push_back(task);
}

std::list<FreeRTOSTask *> TaskManager::getFreeRTOSTasks() {
  return _FreeRTOSTasks;
}

FreeRTOSTask::FreeRTOSTask(const String &name, int taskId, UBaseType_t priority, uint32_t stackDepth, BaseType_t coreId, const bool displayOnScreen) : _state(Okay), _stateInfo("Booting"), taskStarted(false), _name(name), _taskId(taskId), _priority(priority), _stackDepth(stackDepth), _coreId(coreId), _displayOnScreen(displayOnScreen) {
}

FreeRTOSTask::~FreeRTOSTask() {
#if INCLUDE_vTaskDelete
  vTaskDelete(handle);
#endif
  return;
}

void FreeRTOSTask::taskWrap(void *param) {
  static_cast<FreeRTOSTask *>(param)->worker();
#if INCLUDE_vTaskDelete
  vTaskDelete(static_cast<FreeRTOSTask *>(param)->handle);
#else
  while (1) {
    vTaskDelay(10000);
  }
#endif
}

bool FreeRTOSTask::start() {
  if (taskStarted) {
    return false;
  } else {
    xTaskCreateUniversal(this->taskWrap, getName(), _stackDepth, this, _priority, &handle, _coreId);
    return true;
  }
}
