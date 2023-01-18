#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskMQTT.h"
#include "TaskRouter.h"
#include "project_configuration.h"

RouterTask::RouterTask(UBaseType_t priority, BaseType_t coreId, System &system, TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<std::shared_ptr<APRSMessage>> &toMQTT) : FreeRTOSTask(TASK_ROUTER, TaskRouter, priority, 2048, coreId), _fromModem(fromModem), _toModem(toModem), _toAprsIs(toAprsIs), _toMQTT(toMQTT) {
  _system = &system;
  start();
}

RouterTask::~RouterTask() {
}

void RouterTask::worker() {
  for (;;) {
    while (!_fromModem.empty()) {
      std::shared_ptr<APRSMessage> modemMsg = _fromModem.getElement();

      if (_system->getUserConfig()->mqtt.active) {
        _toMQTT.addElement(modemMsg);
      }

      if (_system->getUserConfig()->aprs_is.active && modemMsg->getSource() != _system->getUserConfig()->callsign) {
        std::shared_ptr<APRSMessage> aprsIsMsg = std::make_shared<APRSMessage>(*modemMsg);
        String                       path      = aprsIsMsg->getPath();

        if (!(path.indexOf("RFONLY") != -1 || path.indexOf("NOGATE") != -1 || path.indexOf("TCPIP") != -1)) {
          if (!path.isEmpty()) {
            path += ",";
          }

          aprsIsMsg->setPath(path + "qAO," + _system->getUserConfig()->callsign);

          APP_LOGI(getName(), "APRS-IS: %s", aprsIsMsg->toString().c_str());
          _toAprsIs.addElement(aprsIsMsg);
        } else {
          APP_LOGI(getName(), "APRS-IS: no forward => RFonly");
        }
      } else {
        if (!_system->getUserConfig()->aprs_is.active) {
          APP_LOGI(getName(), "APRS-IS: disabled");
        }

        if (modemMsg->getSource() == _system->getUserConfig()->callsign) {
          APP_LOGI(getName(), "APRS-IS: no forward => own packet received");
        }
      }

      if (_system->getUserConfig()->digi.active && modemMsg->getSource() != _system->getUserConfig()->callsign) {
        std::shared_ptr<APRSMessage> digiMsg = std::make_shared<APRSMessage>(*modemMsg);
        String                       path    = digiMsg->getPath();

        // simple loop check
        if (path.indexOf("WIDE1-1") >= 0 && path.indexOf(_system->getUserConfig()->callsign) == -1) {
          // fixme
          digiMsg->setPath(_system->getUserConfig()->callsign + "*");

          APP_LOGI(getName(), "DIGI: %s", digiMsg->toString().c_str());

          _toModem.addElement(digiMsg);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    _stateInfo = "Router done ";
  }
}
