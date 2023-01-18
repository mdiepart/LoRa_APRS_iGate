#ifndef TASK_ROUTER_H_
#define TASK_ROUTER_H_

#include <APRSMessage.h>
#include <TaskManager.h>

class RouterTask : public FreeRTOSTask {
public:
  RouterTask(UBaseType_t priority, BaseType_t coreId, System &system, TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<std::shared_ptr<APRSMessage>> &toMQTT);
  virtual ~RouterTask();

  void worker() override;

private:
  System                                  *_system;
  TaskQueue<std::shared_ptr<APRSMessage>> &_fromModem;
  TaskQueue<std::shared_ptr<APRSMessage>> &_toModem;
  TaskQueue<std::shared_ptr<APRSMessage>> &_toAprsIs;
  TaskQueue<std::shared_ptr<APRSMessage>> &_toMQTT;
};

#endif
