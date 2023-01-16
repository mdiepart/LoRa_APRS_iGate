#ifndef TASK_ETH_H_
#define TASK_ETH_H_

#include <TaskManager.h>

void NetworkEvent(WiFiEvent_t event);

class EthTask : public FreeRTOSTask {
public:
  EthTask(UBaseType_t priority, BaseType_t coreId, System &system);
  virtual ~EthTask();

  void worker() override;

private:
  System *_system;
};

#endif
