#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <logger.h>

#include "System.h"
#include "Task.h"
#include "TaskOTA.h"
#include "project_configuration.h"

OTATask::OTATask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system) : FreeRTOSTask(TASK_OTA, TaskOta, priority, 2048, coreId), _ota(), _beginCalled(false), _status(OTA_Disabled), _system(system), _enable_time(0), _timeout(0) {
  start();
}

OTATask::~OTATask() {
}

void OTATask::worker() {
  _ota.onStart([&]() {
        String type;
        if (_ota.getCommand() == U_FLASH) {
          type = "sketch";
        } else { // U_SPIFFS
          type = "filesystem";
        }
        APP_LOGI(getName(), "Start updating %s. please wait, this process is taking some time!", type.c_str());
      })
      .onEnd([&]() {
        APP_LOGI(getName(), "OTA End");
      })
      .onError([&](ota_error_t error) {
        String error_str;
        if (error == OTA_AUTH_ERROR) {
          error_str = "Auth Failed";
        } else if (error == OTA_BEGIN_ERROR) {
          error_str = "Begin Failed";
        } else if (error == OTA_CONNECT_ERROR) {
          error_str = "Connect Failed";
        } else if (error == OTA_RECEIVE_ERROR) {
          error_str = "Receive Failed";
        } else if (error == OTA_END_ERROR) {
          error_str = "End Failed";
        }
        APP_LOGE(getName(), "Error[%d]: %s", error, error_str.c_str());
      });
  if (_system.getUserConfig()->network.hostname.overwrite) {
    _ota.setHostname(_system.getUserConfig()->network.hostname.name.c_str());
  } else {
    _ota.setHostname(_system.getUserConfig()->callsign.c_str());
  }

  if (!_system.getUserConfig()->ota.password.isEmpty()) {
    _ota.setPassword(_system.getUserConfig()->ota.password.c_str());
    APP_LOGI(getName(), "Set OTA password to %s", _system.getUserConfig()->ota.password.c_str());
  }

  _ota.setPort((uint16_t)_system.getUserConfig()->ota.port);
  APP_LOGI(getName(), "Set OTA port to %u", _system.getUserConfig()->ota.port);

  if (_system.getUserConfig()->ota.active) {
    if (_system.getUserConfig()->ota.enableViaWeb) {
      _stateInfo = "Web";
      _status    = OTA_Disabled;
    } else {
      _stateInfo = "Okay";
      _status    = OTA_ForceEnabled;
    }
  } else {
    _stateInfo = "Disabled";
    _status    = OTA_ForceDisabled;
  }

  for (;;) {
    if (_status == OTA_Enabled && (millis() - _enable_time >= _timeout)) {
      _status = OTA_Disabled;
      APP_LOGI(getName(), "OTA Timed out. Disabling OTA.");
    }

    if (!_beginCalled && (_status == OTA_ForceEnabled || _status == OTA_Enabled)) {
      _ota.begin();
      _beginCalled = true;
    }

    if (_beginCalled && (_status == OTA_ForceDisabled || _status == OTA_Disabled)) {
      _ota.end();
      _beginCalled = false;
    }

    if (_beginCalled && (_status == OTA_ForceEnabled || _status == OTA_Enabled)) {
      _ota.handle();
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

OTATask::Status OTATask::getOTAStatus() {
  return _status;
}

void OTATask::enableOTA(unsigned int timeout) {
  if (_status == OTA_Disabled || _status == OTA_Enabled) {
    _enable_time = millis();
    _timeout     = timeout;
    _status      = OTA_Enabled;
  }
}

/**
 * @brief Returns the time remaining before OTA times out and disables itself
 *
 * @return unsigned int the time left in milliseconds
 */
unsigned int OTATask::getTimeRemaining() {
  if (_status == OTA_Enabled) {
    return (unsigned long)_timeout - millis() + _enable_time;
  }
  return 0;
}
