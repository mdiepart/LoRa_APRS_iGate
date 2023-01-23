#ifndef TASK_ROUTER_H_
#define TASK_ROUTER_H_

#include <APRSMessage.h>
#include <TaskManager.h>

class RouterTask : public FreeRTOSTask {
public:
  RouterTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system, QueueHandle_t &fromModem, QueueHandle_t &toModem, QueueHandle_t &toAprsIs, QueueHandle_t &toMQTT);
  virtual ~RouterTask();

  void worker() override;

private:
  System        &_system;
  QueueHandle_t &_fromModem;
  QueueHandle_t &_toModem;
  QueueHandle_t &_toAprsIs;
  QueueHandle_t &_toMQTT;
};

#endif
