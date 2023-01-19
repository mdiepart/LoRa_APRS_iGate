#include <logger.h>

#include "Task.h"
#include "TaskDisplay.h"
#include "project_configuration.h"

DisplayTask::DisplayTask(UBaseType_t priority, BaseType_t coreId, System &system) : FreeRTOSTask(TASK_DISPLAY, TaskDisplay, priority, 2048, coreId), _system(system) {
  _system.getDisplay().setup(_system.getBoardConfig());
  start();
}

DisplayTask::~DisplayTask() {
}

void DisplayTask::worker() {
  vTaskDelay(pdMS_TO_TICKS(100));
  if (_system.getUserConfig()->display.turn180) {
    _system.getDisplay().turn180();
  }

  std::shared_ptr<StatusFrame> statusFrame = std::shared_ptr<StatusFrame>(new StatusFrame(_system.getTaskManager().getFreeRTOSTasks()));
  _system.getDisplay().setStatusFrame(statusFrame);

  if (!_system.getUserConfig()->display.alwaysOn) {
    _system.getDisplay().activateDisplaySaveMode();
    _system.getDisplay().setDisplaySaveTimeout(_system.getUserConfig()->display.timeout);
  }
  _stateInfo = _system.getUserConfig()->callsign;

  for (;;) {
    if (_system.getUserConfig()->display.overwritePin != 0 && !digitalRead(_system.getUserConfig()->display.overwritePin)) {
      _system.getDisplay().activateDistplay();
    }
    _system.getDisplay().update();
    vTaskDelay(501 / portTICK_PERIOD_MS); // 501 just because there is an internal 500ms timer in display::update()
  }
}
