#ifndef TASK_WEB_H_
#define TASK_WEB_H_

#include <TaskManager.h>
#include <WiFiMulti.h>
#include <map>

class WebTask : public Task {
public:
  WebTask();
  virtual ~WebTask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

private:
  struct session_cookie {
    session_cookie(unsigned long timestamp, String value);
    String        value;     // Session id (64) + terminator
    unsigned long timestamp; // Timestamp (in ms) when the cookie was last created / refreshed

    String creationString();
  };

  WiFiServer         http_server;
  const unsigned int TIMEOUT          = 5;      // Timeout in s
  const unsigned int SESSION_LIFETIME = 900000; // user session lifetime in ms (900 000 is 15 minutes)
  const String       STATUS_200       = String("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String       STATUS_303_INFO  = String("HTTP/1.1 303 See Other\r\nLocation: /info\r\nContent-type:text/html\r\nConnection: close\r\n");

  std::map<uint32_t, struct session_cookie> connected_clients; // Key is ip address, value is cookie

  String loadPage(String file);
  String readRequestHeader(WiFiClient &client);
  String readRequest(WiFiClient &client);
  bool   isClientLoggedIn(const WiFiClient &client, const String &header) const;
  String getSessionCookie(const String &header) const;

  void info_html(String &header, WiFiClient &client, System &system);
  void enableota_html(String &header, WiFiClient &client, System &system);
  void uploadfw_html(String &header, WiFiClient &client, System &system);
  void page_login(String &header, WiFiClient &client, System &system);
};

#endif
