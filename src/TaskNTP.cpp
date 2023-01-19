#include <System.h>
#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskNTP.h"
#include "project_configuration.h"

NTPTask::NTPTask(UBaseType_t priority, BaseType_t coreId, System &system) : FreeRTOSTask(TASK_NTP, TaskNtp, priority, 1748, coreId), _system(system) {
  start();
  APP_LOGD(getName(), "NTP class created.");
}

void NTPTask::worker() {

  _ntpClient.setPoolServerName(_system.getUserConfig()->ntpServer.c_str());
  TickType_t previousWakeTime = xTaskGetTickCount();
  TickType_t wakeInterval     = 3600000 * portTICK_PERIOD_MS; // Every hour
  _ntpClient.begin();
  APP_LOGD(getName(), "NTP Task initialized.");
  for (;;) {
    if (!_system.isWifiOrEthConnected()) {
      _state     = Warning;
      _stateInfo = "Disconnected";
      vTaskDelay(1000 * portTICK_PERIOD_MS);
    } else if (_ntpClient.forceUpdate()) {
      _state     = Okay;
      _stateInfo = _ntpClient.getFormattedTime();
      setTime(_ntpClient.getEpochTime());
      APP_LOGI(getName(), "Current time: %s", _ntpClient.getFormattedTime().c_str());
      xTaskDelayUntil(&previousWakeTime, wakeInterval);
    } else {
      vTaskDelay(1000 * portTICK_PERIOD_MS);
    }
  }
}
