#ifndef TASK_OTA_H_
#define TASK_OTA_H_

#include <ArduinoOTA.h>
#include <TaskManager.h>

class OTATask : public Task {
public:
  OTATask();
  virtual ~OTATask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

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

  uint32_t _enable_time;
  uint32_t _timeout;
};

#endif
