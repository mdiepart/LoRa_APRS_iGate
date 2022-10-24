#ifndef TASK_WEB_H_
#define TASK_WEB_H_

#include <TaskManager.h>
#include <WiFiMulti.h>

class WebTask : public Task {
public:
  WebTask();
  virtual ~WebTask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

private:
  WiFiServer http_server;
  const unsigned int TIMEOUT = 20000; // Timeout in ms
};

#endif
