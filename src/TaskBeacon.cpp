#include <OneButton.h>
#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskBeacon.h"
#include "project_configuration.h"

OneButton BeaconTask::_userButton;
bool      BeaconTask::_send_update;
uint      BeaconTask::_instances;

BeaconTask::BeaconTask(UBaseType_t priority, BaseType_t coreId, System &system, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs) : FreeRTOSTask(TASK_BEACON, TaskBeacon, priority, 2048, coreId), _ss(1), _useGps(false) {
  _system   = &system;
  _toModem  = &toModem;
  _toAprsIs = &toAprsIs;
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

  _useGps    = _system->getUserConfig()->beacon.use_gps;
  _beaconMsg = std::shared_ptr<APRSMessage>(new APRSMessage());
  _beaconMsg->setSource(_system->getUserConfig()->callsign);
  _beaconMsg->setDestination("APLG01");
  _beacon_timer.setTimeout(_system->getUserConfig()->beacon.timeout * 60 * 1000);

  if (_useGps) {
    if (_system->getBoardConfig()->GpsRx != 0) {
      _ss.begin(9600, SERIAL_8N1, _system->getBoardConfig()->GpsTx, _system->getBoardConfig()->GpsRx);
    } else {
      APP_LOGI(getName(), "NO GPS found.");
      _useGps = false;
    }
  }

  for (;;) {
    if (_useGps) {
      while (_ss.available() > 0) {
        char c = _ss.read();
        _gps.encode(c);
      }
    }

    _userButton.tick();

    // check for beacon
    if (_beacon_timer.check() || _send_update) {
      if (sendBeacon()) {
        _send_update = false;
        _beacon_timer.start();
      }
    }

    uint32_t diff = _beacon_timer.getTriggerTimeInSec();
    _stateInfo    = "beacon " + String(uint32_t(diff / 600)) + String(uint32_t(diff / 60) % 10) + ":" + String(uint32_t(diff / 10) % 6) + String(uint32_t(diff % 10));

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

bool BeaconTask::sendBeacon() {
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

  _beaconMsg->getBody()->setData(aprs_data + _system->getUserConfig()->beacon.message);

  APP_LOGI(getName(), "[%s] %s", timeString().c_str(), _beaconMsg->encode().c_str());

  if (_system->getUserConfig()->aprs_is.active) {
    _toAprsIs->addElement(_beaconMsg);
  }

  if (_system->getUserConfig()->digi.beacon) {
    _toModem->addElement(_beaconMsg);
  }

  _system->getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("BEACON", _beaconMsg->toString())));

  return true;
}
