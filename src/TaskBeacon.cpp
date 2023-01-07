#include <OneButton.h>
#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskBeacon.h"
#include "project_configuration.h"

BeaconTask::BeaconTask(TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs) : Task(TASK_BEACON, TaskBeacon), _toModem(toModem), _toAprsIs(toAprsIs), _ss(1), _useGps(false) {
}

BeaconTask::~BeaconTask() {
}

OneButton BeaconTask::_userButton;
bool      BeaconTask::_send_update;
uint      BeaconTask::_instances;

void BeaconTask::pushButton() {
  _send_update = true;
}

bool BeaconTask::setup(System &system) {
  if (_instances++ == 0 && system.getBoardConfig()->Button > 0) {
    _userButton = OneButton(system.getBoardConfig()->Button, true, true);
    _userButton.attachClick(pushButton);
    _send_update = false;
  }

  _useGps = system.getUserConfig()->beacon.use_gps;

  if (_useGps) {
    if (system.getBoardConfig()->GpsRx != 0) {
      _ss.begin(9600, SERIAL_8N1, system.getBoardConfig()->GpsTx, system.getBoardConfig()->GpsRx);
    } else {
      logger.info(getName(), "NO GPS found.");
      _useGps = false;
    }
  }
  // setup beacon
  _beacon_timer.setTimeout(system.getUserConfig()->beacon.timeout * 60 * 1000);

  _beaconMsg = std::shared_ptr<APRSMessage>(new APRSMessage());
  _beaconMsg->setSource(system.getUserConfig()->callsign);
  _beaconMsg->setDestination("APLG01");

  return true;
}

bool BeaconTask::loop(System &system) {
  if (_useGps) {
    while (_ss.available() > 0) {
      char c = _ss.read();
      _gps.encode(c);
    }
  }

  _userButton.tick();

  // check for beacon
  if (_beacon_timer.check() || _send_update) {
    if (sendBeacon(system)) {
      _send_update = false;
      _beacon_timer.start();
    }
  }

  uint32_t diff = _beacon_timer.getTriggerTimeInSec();
  _stateInfo    = "beacon " + String(uint32_t(diff / 600)) + String(uint32_t(diff / 60) % 10) + ":" + String(uint32_t(diff / 10) % 6) + String(uint32_t(diff % 10));

  return true;
}

bool BeaconTask::sendBeacon(System &system) {
  double lat = system.getUserConfig()->beacon.positionLatitude;
  double lng = system.getUserConfig()->beacon.positionLongitude;

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

  _beaconMsg->getBody()->setData(aprs_data + system.getUserConfig()->beacon.message);

  logger.info(getName(), "[%s] %s", timeString().c_str(), _beaconMsg->encode().c_str());

  if (system.getUserConfig()->aprs_is.active) {
    _toAprsIs.addElement(_beaconMsg);
  }

  if (system.getUserConfig()->digi.beacon) {
    _toModem.addElement(_beaconMsg);
  }

  system.getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("BEACON", _beaconMsg->toString())));

  return true;
}
