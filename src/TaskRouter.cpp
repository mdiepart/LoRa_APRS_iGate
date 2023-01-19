#include <TimeLib.h>
#include <logger.h>

#include "Task.h"
#include "TaskMQTT.h"
#include "TaskRouter.h"
#include "project_configuration.h"

RouterTask::RouterTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &fromModem, QueueHandle_t &toModem, QueueHandle_t &toAprsIs, QueueHandle_t &toMQTT) : FreeRTOSTask(TASK_ROUTER, TaskRouter, priority, 2048, coreId), _fromModem(fromModem), _toModem(toModem), _toAprsIs(toAprsIs), _toMQTT(toMQTT) {
  _system = &system;
  start();
}

RouterTask::~RouterTask() {
}

void RouterTask::worker() {
  for (;;) {
    while (uxQueueMessagesWaiting(_fromModem) > 0) {
      APRSMessage *fromModemMsg;
      xQueueReceive(_fromModem, &fromModemMsg, 0);

      if (_system->getUserConfig()->mqtt.active) {
        APRSMessage *mqttMsg = new APRSMessage(*fromModemMsg);
        xQueueSendToBack(_toMQTT, &mqttMsg, pdMS_TO_TICKS(100));
      }

      if (_system->getUserConfig()->aprs_is.active && fromModemMsg->getSource() != _system->getUserConfig()->callsign) {
        APRSMessage *aprsIsMsg = new APRSMessage(*fromModemMsg);
        String       path      = aprsIsMsg->getPath();

        if (!(path.indexOf("RFONLY") != -1 || path.indexOf("NOGATE") != -1 || path.indexOf("TCPIP") != -1)) {
          if (!path.isEmpty()) {
            path += ",";
          }

          aprsIsMsg->setPath(path + "qAO," + _system->getUserConfig()->callsign);

          APP_LOGI(getName(), "APRS-IS: %s", aprsIsMsg->toString().c_str());
          xQueueSendToBack(_toAprsIs, &aprsIsMsg, pdMS_TO_TICKS(100));
        } else {
          APP_LOGI(getName(), "APRS-IS: no forward => RFonly");
        }
      } else {
        if (!_system->getUserConfig()->aprs_is.active) {
          APP_LOGI(getName(), "APRS-IS: disabled");
        }

        if (fromModemMsg->getSource() == _system->getUserConfig()->callsign) {
          APP_LOGI(getName(), "APRS-IS: no forward => own packet received");
        }
      }

      if (_system->getUserConfig()->digi.active && fromModemMsg->getSource() != _system->getUserConfig()->callsign) {
        APRSMessage *digiMsg = new APRSMessage(*fromModemMsg);

        String path = digiMsg->getPath();

        // simple loop check
        if (path.indexOf("WIDE1-1") >= 0 && path.indexOf(_system->getUserConfig()->callsign) == -1) {
          // fixme
          digiMsg->setPath(_system->getUserConfig()->callsign + "*");

          APP_LOGI(getName(), "DIGI: %s", digiMsg->toString().c_str());

          xQueueSendToBack(_toModem, &digiMsg, pdMS_TO_TICKS(100));
        }
      }
      delete fromModemMsg;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    _stateInfo = "Router done ";
  }
}
