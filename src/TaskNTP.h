#ifndef TASK_NTP_H_
#define TASK_NTP_H_

#include <NTPClient.h>
#include <TaskManager.h>

class NTPTask : public FreeRTOSTask {
public:
  NTPTask(UBaseType_t priority, BaseType_t coreId, int argc, void *argv);

  void worker(int argc, void *argv) override;

private:
  NTPClient _ntpClient;
};

#endif
