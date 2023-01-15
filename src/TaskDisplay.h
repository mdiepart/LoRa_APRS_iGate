#ifndef TASK_DISPLAY_H_
#define TASK_DISPLAY_H_

#include <Display.h>
#include <TaskManager.h>

class DisplayTask : public FreeRTOSTask {
public:
  DisplayTask(UBaseType_t priority, BaseType_t coreId, System &system);
  virtual ~DisplayTask();

  void worker() override;

private:
  System *_system;
};

#endif
