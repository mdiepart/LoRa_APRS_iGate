#include <esp_task_wdt.h>
#include <logger.h>

#include "Task.h"
#include "TaskOTA.h"
#include "project_configuration.h"

OTATask::OTATask() : Task(TASK_OTA, TaskOta), _beginCalled(false) {
}

OTATask::~OTATask() {
}

bool OTATask::setup(System &system) {
  _ota.onStart([&]() {
        String type;
        if (_ota.getCommand() == U_FLASH) {
          type = "sketch";
        } else { // U_SPIFFS
          type = "filesystem";
        }
        system.log_info(getName(), "Start updating %s. please wait, this process is taking some time!", type.c_str());
      })
      .onEnd([&]() {
        system.log_info(getName(), "OTA End");
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
        system.log_error(getName(), "Error[%d]: %s", error, error_str.c_str());
      })
      .onProgress([&](unsigned int received, unsigned int total_size) {
        esp_task_wdt_reset();
      });
  if (system.getUserConfig()->network.hostname.overwrite) {
    _ota.setHostname(system.getUserConfig()->network.hostname.name.c_str());
  } else {
    _ota.setHostname(system.getUserConfig()->callsign.c_str());
  }

  if (!system.getUserConfig()->ota.password.isEmpty()) {
    _ota.setPassword(system.getUserConfig()->ota.password.c_str());
    system.log_info(getName(), "Set OTA password to %s", system.getUserConfig()->ota.password.c_str());
  }

  _ota.setPort((uint16_t)system.getUserConfig()->ota.port);
  system.log_info(getName(), "Set OTA port to %u", system.getUserConfig()->ota.port);

  if (system.getUserConfig()->ota.active) {
    if (system.getUserConfig()->ota.enableViaWeb) {
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

  return true;
}

bool OTATask::loop(System &system) {

  if (_status == OTA_Enabled && (millis() - _enable_time >= _timeout)) {
    _status = OTA_Disabled;
    system.log_info(getName(), "OTA Timed out. Disabling OTA.");
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

  return true;
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
