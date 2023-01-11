#ifndef TASK_FTP_H_
#define TASK_FTP_H_

#include <ESP-FTP-Server-Lib.h>
#include <TaskManager.h>

class FTPTask : public FreeRTOSTask {
public:
  FTPTask(UBaseType_t priority, BaseType_t coreId, int argc, void *argv);

  void worker(int argc, void *argv) override;

private:
  FTPServer _ftpServer;
};

#endif
