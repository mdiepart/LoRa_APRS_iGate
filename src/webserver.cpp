#include <WiFiMulti.h>
#include <map>
#include <set>
#include <utility>

#include "webserver.h"

webserver::webserver() {
  get_targets     = std::map<String, targetProcessing>();
  post_targets    = std::map<String, targetProcessing>();
  known_resources = std::set<String>();
}

bool webserver::addTarget(Method m, const String target, targetProcessing fn) {
  if (m == GET) {
    if (get_targets.find(target) != get_targets.end()) {
      get_targets.erase(target);
    }
    get_targets.emplace(target, fn);
    known_resources.emplace(target);
    return true;
  } else if (m == POST) {
    if (post_targets.find(target) != post_targets.end()) {
      post_targets.erase(target);
    }
    post_targets.emplace(target, fn);
    known_resources.emplace(target);
    return true;
  }

  return false;
}

void webserver::serve(WiFiClient &client, System &system) {
  String start_line = client.readStringUntil('\n');
  start_line += '\n';

  logger.info(String("webServer"), "start line : %s", start_line.c_str());

  webTarget target(start_line);

  std::map<String, targetProcessing>::iterator found_target;

  if (target.getMethod() == webTarget::INVALID) {
    // Error 400 Bad Request
    client.println(STATUS_400);
    return;
  } else if (target.getMethod() == webTarget::NOT_SUPPORTED) {
    // Error 501 Not Implemented
    client.println(STATUS_501);
    return;
  }

  // If target is unknown, return 404
  if (known_resources.find(target.getResource()) == known_resources.end()) {
    // Error 404 Not Found
    client.println(STATUS_404);
    return;
  } else if (target.getMethod() == webTarget::GET) {
    found_target = get_targets.find(target.getResource());
    if (found_target == get_targets.end()) {
      // This target is not a valid GET target, but is known so it should be a valid POST target
      // Error 405 Method Not Allowed
      client.println(STATUS_405_GET);
      return;
    }
  } else if (target.getMethod() == webTarget::POST) {
    found_target = post_targets.find(target.getResource());
    if (found_target == post_targets.end()) {
      // This target is not a valid POST target, but is known so it should be a valid GET target
      // Error 405 Method Not Allowed
      client.println(STATUS_405_POST);
      return;
    }
  } else {
    logger.info(String("webServer"), "Found weird target, we should not be here...");

    // Error 500
    client.println(STATUS_500);
    return;
  }

  // Read header
  String headerStr = readHeader(client);
  if (headerStr.length() > MAX_HEADER_LENGTH) {
    client.println(STATUS_413);
    return;
  }

  // Parse header
  Header_t header = parseHeader(start_line, headerStr);

  // Call function
  found_target->second(client, header, system);
  return;
}

String webserver::readHeader(WiFiClient &client) {
  String        ret        = "";
  unsigned long start_time = millis();
  size_t        header_len = 0;
  while (client.available() && (millis() - start_time < TIMEOUT * 1000) && header_len <= MAX_HEADER_LENGTH) {
    char c = (char)client.read(); // Should succeed because client.available returned true
    ret += c;
    header_len++;
    if (c == '\r') {
      // We might be at the end of the header. Check for that.
      char lfcrlf[4] = {0};

      client.readBytes(lfcrlf, 3);
      ret += lfcrlf;
      if (strcmp(lfcrlf, "\n\r\n") == 0) {
        return ret;
      }
    }
  }

  return ret;
}

webserver::Header_t webserver::parseHeader(String start_line, String str) {
  Header_t header = Header_t();
  header[""]      = start_line; // Insert the header with special value key ""

  while (str.length() > 0) {
    String line, key, val;
    line = str.substring(0, str.indexOf("\n")) + '\n';
    str.remove(0, line.length());

    key = line.substring(0, line.indexOf(":"));
    val = line.substring(key.length() + 1, line.length());
    val.trim();
    header[key] = val;
  }

  return header;
}

webserver &webserver::operator=(const webserver &other) {
  this->get_targets     = other.get_targets;
  this->post_targets    = other.post_targets;
  this->known_resources = other.known_resources;
  return *this;
}

webTarget::webTarget(String line) {
  if (line.startsWith("GET ")) {
    method   = GET;
    resource = line.substring(4, line.indexOf(" HTTP/"));
    version  = line.substring(line.indexOf(" HTTP/") + 6, line.indexOf("\r\n"));
  } else if (line.startsWith("POST ")) {
    method   = POST;
    resource = line.substring(5, line.indexOf(" HTTP/"));
    version  = line.substring(line.indexOf(" HTTP/") + 6, line.indexOf("\r\n"));
  } else {
    method = NOT_SUPPORTED;
  }
}

webTarget::Method webTarget::getMethod() {
  return method;
}

String webTarget::getResource() {
  return resource;
}

String webTarget::getVersion() {
  return version;
}
