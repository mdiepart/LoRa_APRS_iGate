#ifndef TASK_NTP_H_
#define TASK_NTP_H_

#include <NTPClient.h>
#include <System.h>
#include <TaskManager.h>

class NTPTask : public FreeRTOSTask {
public:
  NTPTask(UBaseType_t priority, BaseType_t coreId, System &system);

  void worker() override;

private:
  System   &_system;
  NTPClient _ntpClient;
};

#endif
