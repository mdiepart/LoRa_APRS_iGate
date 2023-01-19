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

// cppcheck-suppress unusedFunction
void StatusFrame::drawStatusPage(Bitmap &bitmap) {
  int y = 0;
  for (FreeRTOSTask *task : _tasks) {
    int x = bitmap.drawString(0, y, (String(task->getName())).substring(0, String(task->getName()).indexOf("Task")));
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
