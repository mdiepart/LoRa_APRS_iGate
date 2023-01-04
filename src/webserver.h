#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#include <WiFiMulti.h>
#include <map>
#include <set>

#include "System.h"

/**
 * @brief Class containing the webserver.
 *
 * It only serves pages and handles accordingly the codes for
 *
 */
class webserver {

public:
  typedef enum {
    NOT_SUPPORTED,
    GET,
    POST,
  } Method;

  /**
   * @brief Map containing an HTTP header
   *
   * Each option is stored as a key whose value is the value passed in the header.
   * The special key corresponding to an empty string correspond to the start line
   * of the header (e.g. GET / HTTP/1.1)
   */
  typedef std::map<String, String>                                Header_t;
  typedef std::function<void(WiFiClient &, Header_t &, System &)> targetProcessing;

  webserver();
  void serve(WiFiClient &client, System &system);
  bool addTarget(Method m, const String target, targetProcessing fn);
  // TODO
  webserver &operator=(const webserver &other);

private:
  const unsigned int TIMEOUT           = 5; // Timeout in s
  const size_t       MAX_HEADER_LENGTH = 1024;

  const String STATUS_400      = String("HTTP/1.1 400 Bad Request\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String STATUS_404      = String("HTTP/1.1 404 Not Found\r\nContent-type:text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html><html>"
                                             "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                                             "<link rel=\"icon\" href=\"data:,\"><body><h1>404 Not Found</h1></body></html>\r\n");
  const String STATUS_405_GET  = String("HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String STATUS_405_POST = String("HTTP/1.1 405 Method Not Allowed\r\nAllow: POST\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String STATUS_413      = String("HTTP/1.1 413 Payload Too Large\r\nContent-type:text/html\r\nConnection: close\r\n");

  const String STATUS_500 = String("HTTP/1.1 500 Internal Server Error\r\nContent-type:text/html\r\nConnection: close\r\n");
  const String STATUS_501 = String("HTTP/1.1 500 Not Implemented Error\r\nContent-type:text/html\r\nConnection: close\r\n");

  std::map<String, targetProcessing> get_targets;
  std::map<String, targetProcessing> post_targets;
  std::set<String>                   known_resources;
  System                             system;

  String   readHeader(WiFiClient &client);
  Header_t parseHeader(String start_line, String str);
};

class webTarget {
public:
  webTarget(String line);

  enum Method {
    NOT_SUPPORTED = 0,
    INVALID,
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
