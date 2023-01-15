#ifndef TASK_APRS_IS_H_
#define TASK_APRS_IS_H_

#include <APRS-IS.h>
#include <APRSMessage.h>
#include <TaskManager.h>

class AprsIsTask : public FreeRTOSTask {
public:
  explicit AprsIsTask(UBaseType_t priority, BaseType_t coreId, System &system, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs);
  virtual ~AprsIsTask();

  void worker() override;

private:
  APRS_IS _aprs_is;

  TaskQueue<std::shared_ptr<APRSMessage>> *_toAprsIs;
  System                                  *_system;
  bool                                     connect();
};

#endif
