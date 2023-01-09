#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskNTP.h"
#include "project_configuration.h"

NTPTask::NTPTask(UBaseType_t priority, BaseType_t coreId, int argc, void *argv) : FreeRTOSTask(TASK_NTP, TaskNtp, priority, 1684, coreId, argc, argv) {
  logger.debug(getName(), "NTP class created.");
}

void NTPTask::worker(int argc, void *argument) {
  configASSERT(argc == 1);
  System *system = static_cast<System *>(argument);
  _ntpClient.setPoolServerName(system->getUserConfig()->ntpServer.c_str());
  TickType_t previousWakeTime = xTaskGetTickCount();
  TickType_t wakeInterval     = 3600000 * portTICK_PERIOD_MS; // Every hour
  _ntpClient.begin();
  logger.debug(getName(), "NTP Task initialized.");
  for (;;) {
    if (!system->isWifiOrEthConnected()) {
      _state     = Warning;
      _stateInfo = "Disconnected";
      vTaskDelay(1000 * portTICK_PERIOD_MS);
    } else if (_ntpClient.forceUpdate()) {
      _state     = Okay;
      _stateInfo = _ntpClient.getFormattedTime();
      setTime(_ntpClient.getEpochTime());
      logger.info(getName(), "Current time: %s", _ntpClient.getFormattedTime().c_str());
      xTaskDelayUntil(&previousWakeTime, wakeInterval);
    } else {
      vTaskDelay(1000 * portTICK_PERIOD_MS);
    }
  }
}
