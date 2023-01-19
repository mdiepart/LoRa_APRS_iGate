#ifndef TASK_MQTT_H_
#define TASK_MQTT_H_

#include <APRSMessage.h>
#include <PubSubClient.h>
#include <TaskManager.h>
#include <WiFi.h>

class MQTTTask : public FreeRTOSTask {
public:
  MQTTTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &toMQTT);

  void worker() override;

private:
  System        &_system;
  WiFiClient     _client;
  QueueHandle_t &_toMQTT;
  PubSubClient   _MQTT;
};

#endif
