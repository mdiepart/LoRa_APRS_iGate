#ifndef TASK_FTP_H_
#define TASK_FTP_H_

#include <ESP-FTP-Server-Lib.h>
#include <TaskManager.h>

class FTPTask : public FreeRTOSTask {
public:
  FTPTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system);

  void worker() override;

private:
  System   &_system;
  FTPServer _ftpServer;
};

#endif
