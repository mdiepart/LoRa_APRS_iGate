#include <OneButton.h>
#include <ctime>
#include <esp_sntp.h>
#include <functional>
#include <logger.h>

#include "Task.h"
#include "TaskBeacon.h"
#include "project_configuration.h"

OneButton         BeaconTask::_userButton;
bool              BeaconTask::_send_update;
bool              BeaconTask::_fast_pace;
uint              BeaconTask::_instances;
TaskHandle_t      beaconTaskhandle = NULL;
SemaphoreHandle_t buttonSemaphore;

BeaconTask::BeaconTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &toModem, QueueHandle_t &toAprsIs) : FreeRTOSTask(TASK_BEACON, TaskBeacon, priority, 2048, coreId), _toModem(toModem), _toAprsIs(toAprsIs), _system(system), _ss(1), _useGps(false) /*, _beaconMsgReady(false), _aprsBeaconSent(false)*/, _lastBeaconSentTime(0), _beaconPeriod(pdMS_TO_TICKS(_system.getUserConfig()->beacon.timeout * 60 * 1000)), _fast_pace_start_time(0) {
  start();
}

void BeaconTask::pushButton() {
  _send_update = true;
}

void BeaconTask::startFastPace() {
  BaseType_t higherPriorityAwoken = pdFALSE;
  if (_fast_pace == false) {
    _fast_pace = true;
    xSemaphoreGiveFromISR(buttonSemaphore, &higherPriorityAwoken);
    portYIELD_FROM_ISR(higherPriorityAwoken);
  }
}

void BeaconTask::worker() {
  time_t    now;
  struct tm timeInfo;
  char      timeStr[9];
  buttonSemaphore = xSemaphoreCreateBinary();

  if (buttonSemaphore == NULL) {
    APP_LOGE(getName(), "Could not create semaphore. Beacon button disabled.");
  } else if (_instances++ == 0 && _system.getBoardConfig()->Button > 0) {
    attachInterrupt(_system.getBoardConfig()->Button, startFastPace, CHANGE);
    _userButton = OneButton(_system.getBoardConfig()->Button, true, false);
    _userButton.attachClick(pushButton);
    _send_update = false;
  }

  _useGps = _system.getUserConfig()->beacon.use_gps;
  _beaconMsg.setSource(_system.getUserConfig()->callsign);
  _beaconMsg.setDestination("APLG01");

  if (_useGps) {
    if (_system.getBoardConfig()->GpsRx != 0) {
      _ss.begin(9600, SERIAL_8N1, _system.getBoardConfig()->GpsTx, _system.getBoardConfig()->GpsRx);
      auto onReceiveFn = std::bind(std::mem_fn(&BeaconTask::onGpsSSReceive), this);
      _ss.onReceive(onReceiveFn);
    } else {
      APP_LOGW(getName(), "NO GPS found.");
      _useGps = false;
    }
  }

  // Wait for network and NTP time
  if (_system.getUserConfig()->aprs_is.active) {
    while (!_system.isWifiOrEthConnected() || (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)) {
      vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
  }

  _send_update = true;
  for (;;) {
    // Send update if needed
    if (_send_update) {
      if (buildBeaconMsg() == false) {
        _send_update = false;
        continue;
      }
      _stateInfo          = "Sending...";
      _lastBeaconSentTime = xTaskGetTickCount();
      if (_system.getUserConfig()->aprs_is.active) {
        // Prepare APRS message to send to APRS-is
        APRSMessage *ipMsg = new APRSMessage(_beaconMsg);
        xQueueSendToBack(_toAprsIs, &ipMsg, pdMS_TO_TICKS(100));

        // Log
        time(&now);
        localtime_r(&now, &timeInfo);
        strftime(timeStr, 9, "%T", &timeInfo);
        APP_LOGI(getName(), "[IP Beacon][%s] %s", timeStr, _beaconMsg.encode().c_str());
        _system.getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("IP BEACON", _beaconMsg.toString())));

        // Wait at least 30s before RF
        if (_system.getUserConfig()->digi.beacon) {
          vTaskDelay(pdMS_TO_TICKS(30000));
        }
      }
      if (_system.getUserConfig()->digi.beacon) {
        // Prepare APRS message to send to RF
        APRSMessage *rfMsg = new APRSMessage(_beaconMsg);
        xQueueSendToBack(_toModem, &rfMsg, pdMS_TO_TICKS(100));

        // Log
        time(&now);
        localtime_r(&now, &timeInfo);
        strftime(timeStr, 9, "%T", &timeInfo);
        APP_LOGI(getName(), "[RF Beacon][%s] %s", timeStr, _beaconMsg.encode().c_str());
        _system.getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("RF BEACON", _beaconMsg.toString())));
      }
      _stateInfo   = "Beacon Sent";
      _send_update = false;
    }

    // Check button if needed
    if (_fast_pace) {
      _userButton.tick();
      vTaskDelay(pdMS_TO_TICKS(10));
      if (xTaskGetTickCount() - _fast_pace_start_time >= _fast_pace_timeout) {
        _fast_pace = false;
      }
      continue;
    } else {
      TickType_t ticksLeft = _beaconPeriod - (xTaskGetTickCount() - _lastBeaconSentTime);
      time(&now);
      now += ((ticksLeft + 999) / 1000);
      localtime_r(&now, &timeInfo);
      strftime(timeStr, sizeof(timeStr), "%T", &timeInfo);
      _stateInfo = String("Next @ ") + timeStr;
      if (xSemaphoreTake(buttonSemaphore, ticksLeft) == pdTRUE) {
        // Semaphore was obtained. That means that the button was pressed.
        _fast_pace            = true;
        _fast_pace_start_time = xTaskGetTickCount();
      } else {
        // Timed out, we need to send a beacon
        _send_update = true;
      }
    }
  }
}

void BeaconTask::onGpsSSReceive() {
  uint8_t buffer[64];
  size_t  n = min<size_t>(_ss.available(), 64);
  n         = _ss.readBytes(buffer, n);

  for (size_t i = 0; i < n; i++) {
    _gps.encode(buffer[i]);
  }
}

bool BeaconTask::buildBeaconMsg() {
  double lat = _system.getUserConfig()->beacon.positionLatitude;
  double lng = _system.getUserConfig()->beacon.positionLongitude;

  if (_useGps) {
    if (_gps.location.isUpdated()) {
      lat = _gps.location.lat();
      lng = _gps.location.lng();
    } else {
      return false;
    }
  }

  String   aprs_data = "!L";
  uint32_t lat_91    = 380926 * (90.0 - lat);
  uint32_t lng_91    = 190463 * (180.0 + lng);

  aprs_data += String(char(33 + (lat_91 / 753571)));
  lat_91 %= 753571;
  aprs_data += String(char(33 + (lat_91 / 8281)));
  lat_91 %= 8281;
  aprs_data += String(char(33 + (lat_91 / 91)));
  lat_91 %= 91;
  aprs_data += String(char(33 + lat_91));

  aprs_data += String(char(33 + (lng_91 / 753571)));
  lng_91 %= 753571;
  aprs_data += String(char(33 + (lng_91 / 8281)));
  lng_91 %= 8281;
  aprs_data += String(char(33 + (lng_91 / 91)));
  lng_91 %= 91;
  aprs_data += String(char(33 + lng_91));

  aprs_data += "& sT"; // No course, speed, range or compression type byte

  _beaconMsg.getBody()->setData(aprs_data + _system.getUserConfig()->beacon.message);

  return true;
}
