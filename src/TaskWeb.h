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
  const unsigned int TIMEOUT = 5; // Timeout in s
  const String STATUS_200 = String("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n");

  String loadPage(String file);
  void info_html(String &header, WiFiClient &client, System &system);
  void enableota_html(String &header, WiFiClient &client, System &system);
  void uploadfw_html(String &header, WiFiClient &client, System &system);

  String readRequestHeader(WiFiClient &client);
  String readRequest(WiFiClient &client);
};

#endif
