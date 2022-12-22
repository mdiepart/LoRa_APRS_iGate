#include <logger.h>

#include "Task.h"
#include "TaskAprsIs.h"
#include "project_configuration.h"

AprsIsTask::AprsIsTask(TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs) : Task(TASK_APRS_IS, TaskAprsIs), _toAprsIs(toAprsIs) {
}

AprsIsTask::~AprsIsTask() {
}

bool AprsIsTask::setup(System &system) {
  _aprs_is.setup(system.getUserConfig()->callsign, system.getUserConfig()->aprs_is.passcode, "ESP32-APRS-IS", "0.2");
  return true;
}

bool AprsIsTask::loop(System &system) {
  if (!system.isWifiOrEthConnected()) {
    return false;
  }
  if (!_aprs_is.connected()) {
    if (!connect(system)) {
      _stateInfo = "not connected";
      _state     = Error;
      return false;
    }
    _stateInfo = "connected";
    _state     = Okay;
    return false;
  }

  _aprs_is.getAPRSMessage();

  if (!_toAprsIs.empty()) {
    std::shared_ptr<APRSMessage> msg = _toAprsIs.getElement();
    _aprs_is.sendMessage(msg);
  }

  return true;
}

bool AprsIsTask::connect(System &system) {
  system.log_info(getName(), "connecting to APRS-IS server: %s on port: %d", system.getUserConfig()->aprs_is.server.c_str(), system.getUserConfig()->aprs_is.port);
  APRS_IS::ConnectionStatus status = _aprs_is.connect(system.getUserConfig()->aprs_is.server, system.getUserConfig()->aprs_is.port);
  if (status == APRS_IS::ERROR_CONNECTION) {
    system.log_error(getName(), "Something went wrong on connecting! Is the server reachable?");
    system.log_error(getName(), "Connection failed.");
    return false;
  } else if (status == APRS_IS::ERROR_PASSCODE) {
    system.log_error(getName(), "User can not be verified with passcode!");
    system.log_error(getName(), "Connection failed.");
    return false;
  }
  system.log_info(getName(), "Connected to APRS-IS server!");
  return true;
}
