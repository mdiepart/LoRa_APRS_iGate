#ifndef TASK_OTA_H_
#define TASK_OTA_H_

#include "System.h"
#include <ArduinoOTA.h>
#include <TaskManager.h>

class OTATask : public FreeRTOSTask {
public:
  OTATask(UBaseType_t priority, BaseType_t coreId, System &system);
  virtual ~OTATask();

  void worker() override;

  enum Status {
    OTA_ForceDisabled, // OTA cannot be enabled
    OTA_ForceEnabled,  // OTA is always enabled
    OTA_Enabled,       // OTA is temporarily enabled
    OTA_Disabled,      // OTA is temporarily disabled
  };

  Status       getOTAStatus();
  void         enableOTA(unsigned int timeout);
  unsigned int getTimeRemaining();

private:
  ArduinoOTAClass _ota;
  bool            _beginCalled;
  Status          _status;
  System         &_system;

  uint32_t _enable_time;
  uint32_t _timeout;
};

#endif
