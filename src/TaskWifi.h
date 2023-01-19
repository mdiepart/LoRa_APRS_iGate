#ifndef TASK_WIFI_H_
#define TASK_WIFI_H_

#include <TaskManager.h>
#include <WiFiMulti.h>

class WifiTask : public FreeRTOSTask {
public:
  WifiTask(UBaseType_t priority, BaseType_t coreId, System &system);

  void worker() override;

private:
  System   &_system;
  WiFiMulti _wiFiMulti;
  uint8_t   _oldWifiStatus;
};

#endif
