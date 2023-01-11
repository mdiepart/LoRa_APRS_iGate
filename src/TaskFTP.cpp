#include <FTPFilesystem.h>
#include <SPIFFS.h>
#include <logger.h>

#include "Task.h"
#include "TaskFTP.h"
#include "project_configuration.h"

FTPTask::FTPTask(UBaseType_t priority, BaseType_t coreId, int argc, void *argv) : FreeRTOSTask(TASK_FTP, TaskFtp, priority, 9216, coreId) {
  /* File buffer inside FTP server is 4096 bytes.*/
  start(argc, argv);
}

void FTPTask::worker(int argc, void *argv) {
  configASSERT(argc == 1);
  System *system = static_cast<System *>(argv);
  for (Configuration::Ftp::User user : system->getUserConfig()->ftp.users) {
    logger.debug(getName(), "Adding user to FTP Server: %s", user.name.c_str());
    _ftpServer.addUser(user.name, user.password);
  }
  _ftpServer.addFilesystem("SPIFFS", &SPIFFS);
  _stateInfo = "waiting";
  while (!system->isWifiOrEthConnected()) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  _ftpServer.begin();

  bool configWasOpen = false;
  for (;;) {
    _ftpServer.handle();
    if (configWasOpen && _ftpServer.countConnections() == 0) {
      logger.warn(getName(), "Maybe the config has been changed via FTP, lets restart now to get the new config...");
      ESP.restart();
    }

    if (_ftpServer.countConnections() > 0) {
      configWasOpen = true;
      _stateInfo    = "Has connection";
      vTaskDelay(100 / portTICK_PERIOD_MS);
    } else if (_ftpServer.countConnections() == 0) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}
