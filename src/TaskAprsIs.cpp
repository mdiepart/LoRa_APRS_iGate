#include <logger.h>

#include "Task.h"
#include "TaskAprsIs.h"
#include "project_configuration.h"

AprsIsTask::AprsIsTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &toAprsIs) : FreeRTOSTask(TASK_APRS_IS, TaskAprsIs, priority, 2048, coreId), _toAprsIs(toAprsIs) {
  _system = &system;
  start();
}

AprsIsTask::~AprsIsTask() {
}

void AprsIsTask::worker() {
  _aprs_is.setup(_system->getUserConfig()->callsign, _system->getUserConfig()->aprs_is.passcode, "ESP32-APRS-IS", "0.2");

  while (!_system->isWifiOrEthConnected()) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  for (;;) {
    if (!_system->isWifiOrEthConnected()) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    if (!_aprs_is.connected()) {
      if (!connect()) {
        _stateInfo = "not connected";
        _state     = Error;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        continue;
      }
      _stateInfo = "connected";
      _state     = Okay;
    }

    _aprs_is.getAPRSMessage();

    if (uxQueueMessagesWaiting(_toAprsIs) > 0) {
      APRSMessage *msg;
      xQueueReceive(_toAprsIs, &msg, 0);
      _aprs_is.sendMessage(msg->encode() + "\n");
      delete msg;
    }

    vTaskDelay(1000);
  }
}

bool AprsIsTask::connect() {
  APP_LOGI(getName(), "connecting to APRS-IS server: %s on port: %d", _system->getUserConfig()->aprs_is.server.c_str(), _system->getUserConfig()->aprs_is.port);
  APRS_IS::ConnectionStatus status = _aprs_is.connect(_system->getUserConfig()->aprs_is.server, _system->getUserConfig()->aprs_is.port);
  if (status == APRS_IS::ERROR_CONNECTION) {
    APP_LOGE(getName(), "Something went wrong on connecting! Is the server reachable?");
    APP_LOGE(getName(), "Connection failed.");
    return false;
  } else if (status == APRS_IS::ERROR_PASSCODE) {
    APP_LOGE(getName(), "User can not be verified with passcode!");
    APP_LOGE(getName(), "Connection failed.");
    return false;
  }
  APP_LOGI(getName(), "Connected to APRS-IS server!");
  return true;
}
