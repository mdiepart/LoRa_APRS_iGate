#include <OneButton.h>
#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskBeacon.h"
#include "project_configuration.h"

OneButton BeaconTask::_userButton;
bool      BeaconTask::_send_update;
uint      BeaconTask::_instances;

BeaconTask::BeaconTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &toModem, QueueHandle_t &toAprsIs) : FreeRTOSTask(TASK_BEACON, TaskBeacon, priority, 2048, coreId), _toModem(toModem), _toAprsIs(toAprsIs), _ss(1), _useGps(false), _beaconMsgReady(false), _aprsBeaconSent(false) {
  _system = &system;
  start();
}

void BeaconTask::pushButton() {
  _send_update = true;
}

void BeaconTask::worker() {
  if (_instances++ == 0 && _system->getBoardConfig()->Button > 0) {
    _userButton = OneButton(_system->getBoardConfig()->Button, true, true);
    _userButton.attachClick(pushButton);
    _send_update = false;
  }

  _useGps = _system->getUserConfig()->beacon.use_gps;
  _beaconMsg.setSource(_system->getUserConfig()->callsign);
  _beaconMsg.setDestination("APLG01");
  _beacon_timer.setTimeout(_system->getUserConfig()->beacon.timeout * 60 * 1000);

  if (_useGps) {
    if (_system->getBoardConfig()->GpsRx != 0) {
      _ss.begin(9600, SERIAL_8N1, _system->getBoardConfig()->GpsTx, _system->getBoardConfig()->GpsRx);
      auto onReceiveFn = std::bind(std::mem_fn(&BeaconTask::onGpsSSReceive), this);
      _ss.onReceive(onReceiveFn);
    } else {
      APP_LOGI(getName(), "NO GPS found.");
      _useGps = false;
    }
  }

  uint32_t aprsSentTime = 0;
  while (!_system->isWifiOrEthConnected()) {
    vTaskDelay(1500 / portTICK_PERIOD_MS);
  }

  for (;;) {
    if (_system->getBoardConfig()->Button > 0) {
      _userButton.tick();
    }

    // check for beacon
    if (_beacon_timer.check() || _send_update) {
      if (buildBeaconMsg()) {
        _beaconMsgReady = true;
        _aprsBeaconSent = false;
        _send_update    = false;
        _beacon_timer.start();
      }
    }

    if (_beaconMsgReady) {
      if (_system->getUserConfig()->aprs_is.active && !_aprsBeaconSent) {
        APRSMessage *ipMsg = new APRSMessage(_beaconMsg);
        xQueueSendToBack(_toAprsIs, &ipMsg, pdMS_TO_TICKS(100));
        APP_LOGI(getName(), "[IP Beacon][%s] %s", timeString().c_str(), _beaconMsg.encode().c_str());
        aprsSentTime    = millis();
        _aprsBeaconSent = true;
        _system->getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("IP BEACON", _beaconMsg.toString())));
      } else if (!_system->getUserConfig()->aprs_is.active) {
        aprsSentTime    = millis() - 30000;
        _aprsBeaconSent = true;
      }

      if (_system->getUserConfig()->digi.beacon) {
        if (millis() - aprsSentTime >= 30000) {
          APRSMessage *rfMsg = new APRSMessage(_beaconMsg);
          xQueueSendToBack(_toModem, &rfMsg, pdMS_TO_TICKS(100));
          APP_LOGI(getName(), "[RF Beacon][%s] %s", timeString().c_str(), _beaconMsg.encode().c_str());
          _system->getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("RF BEACON", _beaconMsg.toString())));
          _beaconMsgReady = false;
        }
      } else {
        _beaconMsgReady = false;
      }
    }

    uint32_t diff = _beacon_timer.getTriggerTimeInSec();
    _stateInfo    = "beacon " + String(uint32_t(diff / 600)) + String(uint32_t(diff / 60) % 10) + ":" + String(uint32_t(diff / 10) % 6) + String(uint32_t(diff % 10));

    vTaskDelay(500 / portTICK_PERIOD_MS);
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
  double lat = _system->getUserConfig()->beacon.positionLatitude;
  double lng = _system->getUserConfig()->beacon.positionLongitude;

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

  _beaconMsg.getBody()->setData(aprs_data + _system->getUserConfig()->beacon.message);

  return true;
}
