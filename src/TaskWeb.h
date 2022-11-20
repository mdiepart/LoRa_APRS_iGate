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
  const String       STATUS_303_INFO  = String("HTTP/1.1 303 See Other\r\nLocation: /info\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String       STATUS_303_LOGIN = String("HTTP/1.1 303 See Other\r\nLocation: /login\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String       STATUS_404       = String("HTTP/1.1 404 Not Found\r\nContent-type:text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html><html>"
                                                           "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                                                           "<link rel=\"icon\" href=\"data:,\"><body><h1>404 Not Found</h1></body></html>\r\n");

  std::map<uint32_t, struct session_cookie> connected_clients; // Key is ip address, value is cookie

  String loadPage(String file);
  String readRequestHeader(WiFiClient &client);
  bool   isClientLoggedIn(const WiFiClient &client, const String &header) const;
  String getSessionCookie(const String &header) const;
  String STATUS_200(WiFiClient &client, const String &header);

  void info_html(String &header, WiFiClient &client, System &system);
  void enableota_html(String &header, WiFiClient &client, System &system);
  void uploadfw_html(String &header, WiFiClient &client, System &system);
  void page_login(String &header, WiFiClient &client, System &system);
};

class webTarget {
public:
  webTarget(String line);

  enum Method {
    NOT_SUPPORTED = 0,
    GET,
    POST,
  };

  enum Method getMethod();
  String      getResource();
  String      getVersion();

private:
  enum Method method;
  String      resource;
  String      version;
};

#endif
