#include <ArduinoJson.h>
#include <logger.h>

#include "Task.h"
#include "TaskMQTT.h"
#include "project_configuration.h"

MQTTTask::MQTTTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &toMQTT) : FreeRTOSTask(TASK_MQTT, TaskMQTT, priority, 3072, coreId), _system(system), _toMQTT(toMQTT), _MQTT(_client) {
  start();
  APP_LOGI(getName(), "MQTT class created.");
}

void MQTTTask::worker() {
  _MQTT.setServer(_system.getUserConfig()->mqtt.server.c_str(), _system.getUserConfig()->mqtt.port);

  for (;;) {
    if (!_system.isWifiOrEthConnected()) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    if (!_MQTT.connected()) {
      if (!_MQTT.connect(_system.getUserConfig()->callsign.c_str(), _system.getUserConfig()->mqtt.name.c_str(), _system.getUserConfig()->mqtt.password.c_str())) {
        APP_LOGI(getName(), "Could not connect to MQTT broker.");
        vTaskDelay(1000);
        continue;
      } else {
        APP_LOGI(getName(), "Connected to MQTT broker as: %s", _system.getUserConfig()->callsign.c_str());
      }
    }

    if (uxQueueMessagesWaiting(_toMQTT) > 0) {
      APRSMessage *msg;
      xQueueReceive(_toMQTT, &msg, pdMS_TO_TICKS(0));

      DynamicJsonDocument data(300);
      data["source"]      = msg->getSource();
      data["destination"] = msg->getDestination();
      data["path"]        = msg->getPath();
      data["type"]        = msg->getType().toString();
      String body         = msg->getBody()->encode();
      body.replace("\n", "");
      data["data"] = body;

      String r;
      serializeJson(data, r);

      String topic = String(_system.getUserConfig()->mqtt.topic);
      if (!topic.endsWith("/")) {
        topic = topic + "/";
      }
      topic = topic + _system.getUserConfig()->callsign;
      APP_LOGD(getName(), "Send MQTT with topic: '%s', data: %s", topic.c_str(), r.c_str());
      _MQTT.publish(topic.c_str(), r.c_str());
      delete msg;
    }

    _MQTT.loop();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
