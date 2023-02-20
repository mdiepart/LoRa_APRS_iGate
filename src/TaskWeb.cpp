#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <functional>
#include <logger.h>

#include "Task.h"
#include "TaskOTA.h"
#include "TaskWeb.h"
#include "project_configuration.h"

WebTask::WebTask() : Task(TASK_WEB, TaskWeb), http_server(80) {
}

WebTask::~WebTask() {
}

bool WebTask::setup(System &system) {
  http_server.begin();
  _stateInfo = "Online";
  // system     = &sys;

  Webserver = webserver();
  {
    using namespace std::placeholders;
    auto fn_root_redirect = std::bind(std::mem_fn(&WebTask::root_redirect), this, _1, _2, _3);
    Webserver.addTarget(webserver::GET, "/", fn_root_redirect); // root. Return 301 -> /info ; no auth

    auto fn_style_css = std::bind(std::mem_fn(&WebTask::style_css), this, _1, _2, _3);
    Webserver.addTarget(webserver::GET, "/style.css", fn_style_css); // style.css; no auth

    auto fn_login = std::bind(std::mem_fn(&WebTask::login_page), this, _1, _2, _3);
    Webserver.addTarget(webserver::GET, "/login", fn_login);  // /login ; no auth
    Webserver.addTarget(webserver::POST, "/login", fn_login); // /login ; no auth

    auto fn_enable_ota = std::bind(std::mem_fn(&WebTask::enableota_page), this, _1, _2, _3);
    Webserver.addTarget(webserver::POST, "/enableOTA", fn_enable_ota); // /enableOTA ; auth

    auto fn_info = std::bind(std::mem_fn(&WebTask::info_page), this, _1, _2, _3);
    Webserver.addTarget(webserver::GET, "/info", fn_info); // /info ; auth

    auto fn_uploadFW = std::bind(std::mem_fn(&WebTask::uploadfw_page), this, _1, _2, _3);
    Webserver.addTarget(webserver::POST, "/uploadFW", fn_uploadFW); // /uploadFW; auth

    auto fn_packet_logs = std::bind(std::mem_fn(&WebTask::download_packets_logs), this, _1, _2, _3);
    Webserver.addTarget(webserver::GET, "/packets.log", fn_packet_logs); // /packets.log; auth
  }

  logger.info(getName(), "Web server started.");
  isServerStarted = true;
  return true;
}

bool WebTask::loop(System &system) {
  if (isServerStarted && !system.isWifiOrEthConnected()) {
    http_server.close();
    isServerStarted = false;
    logger.warn(getName(), "Closed HTTP server because network connection was lost.");
  }

  if (!isServerStarted && system.isWifiOrEthConnected()) {
    http_server.begin();
    isServerStarted = true;
    logger.warn(getName(), "Network connection recovered, http server restarted.");
  }

  // Check for too old client sessions every 10s
  static uint32_t timeSinceRefresh = 0;
  if (millis() - timeSinceRefresh >= 10000) {
    for (auto client : connected_clients) {
      if (millis() - client.second.timestamp > SESSION_LIFETIME) {
        connected_clients.erase(client.first);
      }
    }
  }

  if (!isServerStarted) {
    return true;
  }

  // Check if we have a client available and serve it
  WiFiClient client = http_server.available();

  if (client) {
    client.setTimeout(TIMEOUT);
    logger.info(getName(), "new client with IP %s.", client.localIP().toString().c_str());

    Webserver.serve(client, system);

    // Close the connection
    // client.flush();
    client.stop();
  }

  return true;
}

String WebTask::loadPage(String file) {
  SPIFFS.begin();
  String pageString;
  File   pageFile = SPIFFS.open(file);
  if (!pageFile || !SPIFFS.exists(file)) {
    Serial.println("Could not read required file from spiffs...");
    pageString = String("<!DOCTYPE html><html>"
                        "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                        "<link rel=\"icon\" href=\"data:,\">"
                        "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>"
                        "<body><h1>iGate Web Server</h1>"
                        "<p>Error with filesystem.</p>"
                        "</body></html>");
  } else {
    Serial.println("Accessed file from spiffs...");

    pageString.reserve(pageFile.size());
    while (pageFile.available()) {
      pageString += pageFile.readString();
    }
  }

  return pageString;
}

// TODO ensure that we do not crash because of a header too large (could cause DOS)
String WebTask::readCRLFCRLF(WiFiClient &client) {
  String        ret        = "";
  unsigned long start_time = millis();
  while (client.available() && (millis() - start_time < TIMEOUT * 1000)) {
    char c = (char)client.read(); // Should succeed because client.available returned true
    ret += c;
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

void WebTask::info_page(WiFiClient &client, webserver::Header_t &header, System &system) {
  if (!isClientLoggedIn(client, header)) {
    client.println(STATUS_303_LOGIN);
    return;
  }

  client.println(STATUS_200(client, header));
  String page = loadPage("/info.html");
  page.replace("$$CALLSIGN$$", system.getUserConfig()->callsign);
  page.replace("$$IP$$", client.localIP().toString() + ":" + String(client.localPort()));
  page.replace("$$AP$$", WiFi.SSID());
  page.replace("$$RSSI$$", String(WiFi.RSSI()) + "dBm");

  // Build $$TASKLIST$$ string
  String tasklist = "";
  for (Task *task : system.getTaskManager().getTasks()) {
    switch (task->getTaskId()) {
    case TaskOta:
      switch (((OTATask *)task)->getOTAStatus()) {
      case OTATask::Status::OTA_ForceDisabled:
        tasklist += "<p class=\"task\">" + task->getName() + ": Disabled.</p>";
        break;
      case OTATask::Status::OTA_ForceEnabled:
        tasklist += "<p class=\"task ok\">" + task->getName() + ": Enabled.</p>";
        break;
      case OTATask::Status::OTA_Disabled:
        tasklist += "<p class=\"task warning\">" + task->getName() + ": Disabled. Use web interface to enable.</p>";
        break;
      case OTATask::Status::OTA_Enabled:
        tasklist += "<p class=\"task ok\">" + task->getName() + ": Enabled for ";
        unsigned int seconds = ((OTATask *)task)->getTimeRemaining() / 1000;
        tasklist += String(seconds) + " more seconds.</p>";
        break;
      }
      break;
    case TaskWifi:
      tasklist += "<p class=\"task ok\">" + task->getName() + ": connected to AP \"" + WiFi.SSID() + "\". RSSI is " + String(WiFi.RSSI()) +
                  "dBm "
                  "and IP is " +
                  WiFi.localIP().toString().c_str() + ".</p>";
      break;
    default:
      switch (task->getState()) {
      case TaskDisplayState::Okay:
        tasklist += "<p class=\"task ok\">";
        break;
      case TaskDisplayState::Warning:
        tasklist += "<p class=\"task warning\">";
        break;
      case TaskDisplayState::Error:
        tasklist += "<p class=\"task error\">";
        break;
      }
      tasklist += task->getName() + ": " + task->getStateInfo() + "</p>";
    }
  }
  page.replace("$$TASKLIST$$", tasklist);

  String logs = system.getPacketLogger()->getTail();
  logs.replace("\t\t", "\t \t");
  logs = "<td class=\"tb_body\">" + sanitize(logs) + "</td>";
  logs.replace("&#10;", "</td></tr><tr class=\"tb_row\"><td class=\"tb_body\">"); // Replace NL
  logs.replace("&#9;", "</td><td class=\"tb_body\">");           // Replace TAB
  logs.replace("</tr><tr class=\"tb_row\"><td class=\"tb_body\"></td>", "");      // Remove empty line

  page.replace("$$LOGSLIST$$", logs);

  page.trim();
  client.println(page);
  client.println();
}

void WebTask::download_packets_logs(WiFiClient &client, webserver::Header_t &header, System &system) {
  if (!isClientLoggedIn(client, header)) {
    client.println(STATUS_303_LOGIN);
    return;
  }
  system.getPacketLogger()->getFullLogs(client);
}

void WebTask::enableota_page(WiFiClient &client, webserver::Header_t &header, System &system) {
  if (!isClientLoggedIn(client, header)) {
    client.println(STATUS_303_LOGIN);
    return;
  }
  client.println(STATUS_200(client, header));

  // Load page and replace placeholder
  String page = loadPage("/enableOTA.html");

  std::list<Task *> tasks = system.getTaskManager().getTasks();
  for (Task *it : system.getTaskManager().getTasks()) {
    if (it->getTaskId() == TaskOta) {
      ((OTATask *)it)->enableOTA(5 * 60 * 1000); // Enabling OTA for 5 minutes
      logger.info(getName(), "User enabled OTA for 5 minutes via web interface");
      page.replace("$$STATUS$$", "<p style=\"text-align: center; color: white;\">OTA Enabled for 5 minutes.</p>");

      break;
    }
  }

  page.replace("$$STATUS$$", "<p style=\"text-align: center; color: red;\">There was an error with the OTA task.</p>");

  client.println(page);
}

void WebTask::uploadfw_page(WiFiClient &client, webserver::Header_t &header, System &system) {
  if (!isClientLoggedIn(client, header)) {
    client.println(STATUS_303_LOGIN);
    return;
  }
  const String str_content_type   = "Content-Type: multipart/form-data;";
  const String str_content_length = "Content-Length: ";
  const String str_boundary       = "boundary=";
  String       boundary_token;

  webserver::Header_t::const_iterator it_content_type = header.find("Content-Type");
  if (it_content_type == header.cend()) {
    // This header does not describe data the way we expect it
    logger.debug(getName(), "No content type in header.");
    client.println(STATUS_400);
    return;
  } else {
    // Fetch boundary token
    String content_type = it_content_type->second;
    boundary_token      = "--" + content_type.substring(content_type.indexOf(str_boundary) + str_boundary.length());
    logger.debug(getName(), "The boundary to compare with is %s.", boundary_token.c_str());
  }

  // Finished parsing header. Now parsing form data
  const size_t BUFFER_LENGTH                  = 511;
  uint8_t      full_buffer[BUFFER_LENGTH + 3] = {0}; // add two bytes to allow prepending data when LF was read, add 1 for terminator
  uint8_t     *read_buffer                    = full_buffer + 2;
  full_buffer[1]                              = '\n'; // This char will always be '\n'
  int       len;
  size_t    file_size = 0;
  String    name;
  String    filename;
  esp_err_t esp_error = ESP_OK;

  // Read a line. It should be the first boundary of the form-data
  len                  = client.readBytesUntil('\n', read_buffer, BUFFER_LENGTH);
  read_buffer[len - 1] = '\0'; // Replace '\r' by '\0'
  logger.debug("uploadFW", "boundary token = %s, buffer = %s.", boundary_token.c_str(), read_buffer);
  if (strcmp(boundary_token.c_str(), (const char *)read_buffer) == 0) { // We found a boundary token. It should be at the beginning of a form part
    while (client.available() && esp_error == ESP_OK) {
      String header = readCRLFCRLF(client); // Read the header that is just after the boundary
      // Determine field name
      name     = header.substring(header.indexOf("name=\"") + strlen("name=\""), header.indexOf("\"; filename"));
      filename = header.substring(header.indexOf("filename=\"") + strlen("filename=\""), header.indexOf("\"\r\n"));
      logger.debug(getName(), "Section with name=\"%s\" and filename = \"%s\".", name.c_str(), filename.c_str());

      if (name.equals("Firmware_File")) {
        // if no filename, we skip firmware upload
        if (filename.isEmpty()) {
          logger.debug(getName(), "No firmware file, skipping.");
          continue;
        }

        const esp_partition_t *next_part = esp_ota_get_next_update_partition(NULL);
        esp_ota_handle_t       ota_handle;
        bool                   data_prepended = false;
        esp_error                             = esp_ota_begin(next_part, OTA_SIZE_UNKNOWN, &ota_handle);

        if (esp_error != ESP_OK) {
          logger.warn(getName(), "Error starting OTA.");
          break;
        }

        // While there is data available and we haven't read the boundary token
        while (esp_error == ESP_OK) {
          // Updating is gonna take some time so we need to reset watchdog.
          esp_task_wdt_reset();
          // Read from client if data is available
          if (client.available()) {
            len = client.readBytesUntil('\n', read_buffer, BUFFER_LENGTH);
          } else {
            logger.debug(getName(), "No more bytes available.\n\n\n");
            break;
          }

          if (len == 0) {
            read_buffer[0] = '\n';
            len            = 1 + client.readBytesUntil('\n', read_buffer + 1, BUFFER_LENGTH - 1);
            // system->log_info(getName(), "read with len < 1");
          }

          // If we have read the boundary
          if (len < BUFFER_LENGTH && strstr((const char *)read_buffer, boundary_token.c_str()) != NULL) {
            // Found end boundary
            break; // Escape from parsing loop
          } else {
            uint8_t *data;
            bool     prepend_again = false;

            if (len < BUFFER_LENGTH) {
              // We found a terminator... We store the last byte in case they are the CRLF just before the boundary
              prepend_again = true;
              len--;
            }

            if (data_prepended) {
              // We will be using full_buffer instead of read_buffer
              data = full_buffer;
              len += 2;
              data_prepended = false;
            } else {
              data = read_buffer;
            }

            if (len < 0) {
              logger.info(getName(), "len < 0");
            }
            esp_ota_write(ota_handle, data, len);

            // At this point, if data_prepended is true, that means that the last byte read must be placed in the "prepend" part of the buffer
            if (prepend_again) {
              full_buffer[0] = data[len];
              data_prepended = true;
              prepend_again  = false;
            }
            file_size += len;
          }
        }

        logger.debug(getName(), "Finished parsing binary file. Size is %d", file_size);

        if (esp_error == ESP_OK) {
          esp_error = esp_ota_end(ota_handle);
        } else {
          // esp_ota_abort(ota_handle);
          logger.debug(getName(), "Error while writing to ota. Code is %d", esp_error);
        }

        switch (esp_error) {
        case ESP_OK:
          logger.debug(getName(), "OTA Succeeded.");
          esp_ota_set_boot_partition(next_part);
          break;
        case ESP_ERR_NOT_FOUND:
          logger.warn(getName(), "ESP_ERR_NOT_FOUND.");
          break;
        case ESP_ERR_INVALID_ARG:
          logger.warn(getName(), "ESP_ERR_INVALID_ARG.");
          break;
        case ESP_ERR_OTA_VALIDATE_FAILED:
          logger.warn(getName(), "ESP_ERR_OTA_VALIDATE_FAILED.");
          break;
        case ESP_ERR_INVALID_STATE:
          logger.warn(getName(), "ESP_ERR_INVALID_STATE.");
          break;
        default:
          logger.warn(getName(), "ESP OTA unknown error...");
          break;
        }
      } else if (name.equals("SPIFFS_File")) {
        if (filename.isEmpty()) { // Do not update spiffs if we have no SPIFFS file
          logger.debug(getName(), "No SPIFFS file uploaded. Skipping.");
          continue;
        }

        esp_partition_iterator_t spiffs_part_it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
        const esp_partition_t   *spiffs_part    = esp_partition_get(spiffs_part_it);
        esp_partition_iterator_release(spiffs_part_it);

        bool data_prepended = 0;

        esp_error = esp_partition_erase_range(spiffs_part, 0, spiffs_part->size);
        if (esp_error != ESP_OK) {
          logger.warn(getName(), "Impossible to erase SPIFFS partition...");
          break;
        }

        // While there is data available and we haven't read the boundary token
        while (esp_error == ESP_OK) {
          // Updating is gonna take some time so we need to reset watchdog.
          esp_task_wdt_reset();

          // Read from client if data is available
          if (client.available()) {
            len = client.readBytesUntil('\n', read_buffer, BUFFER_LENGTH);
          } else {
            break;
          }

          // If we have read the boundary
          if (len < BUFFER_LENGTH && strstr((const char *)read_buffer, boundary_token.c_str()) != NULL) {
            // Found end boundary
            break; // Escape from parsing loop
          } else {
            uint8_t *data;
            bool     prepend_again = false;

            if (len < BUFFER_LENGTH) {
              // We found a terminator... We store the last byte in case they are the CRLF just before the boundary
              prepend_again = true;
              len--;
            }

            if (data_prepended) {
              // We will be using full_buffer instead of read_buffer
              len += 2;
              data_prepended = false;
              data           = full_buffer;
            } else {
              data = read_buffer;
            }

            esp_error = esp_partition_write(spiffs_part, file_size, data, len);

            // At this point, if data_prepended is true, that means that the last byte read must be placed in the "prepend" part of the buffer
            if (prepend_again) {
              full_buffer[0] = data[len];
              data_prepended = true;
              prepend_again  = false;
            }
            file_size += len;
          }
        }

        if (esp_error != ESP_OK) {
          logger.warn(getName(), "Error while writing to spiffs partition.");
          break;
        }
      }
    } /* else if ((boundary_token + "--").equals(buffer)) {
       // we found the end of form boundary
       system->log_debug(getName(), "Finished parsing full request.");
       break;
     }*/
  } else {
    // No token where one is expected
    logger.warn(getName(), "Could not find firmware data.");

    client.println(STATUS_400);
    return;
  }

  client.println(STATUS_200(client, header));
  client.println("<!DOCTYPE html><html>"
                 "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                 "<link rel=\"icon\" href=\"data:,\">"
                 "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>"
                 "<body><h1>iGate Web Server</h1>"
                 "<p>OTA Update successful. Please give the device 30s to reboot.</p>"
                 "</body></html>\r\n");

  if (esp_error == ESP_OK) {
    logger.warn(getName(), "ESP OTA succeeded. Restarting in 2s.");
    client.stop();
    esp_restart();
  }
}

void WebTask::login_page(WiFiClient &client, webserver::Header_t &header, System &system) {
  // Check for POST/GET

  webserver::Header_t::const_iterator it_startLine = header.find("");
  if (it_startLine == header.cend()) {
    client.println(STATUS_500);
    return;
  }
  String startLine = it_startLine->second;

  if (startLine.startsWith("GET ")) {
    if (isClientLoggedIn(client, header)) {
      client.println(STATUS_303_INFO);
      return;
    }

    String page = loadPage("/login.html");
    page.replace("$$STATUS$$", "");
    client.println(STATUS_200(client, header));
    client.println(page);
    client.println();
  } else if (startLine.startsWith("POST ")) {
    // Check password
    String body     = readCRLFCRLF(client);
    size_t i        = body.indexOf("Password=");
    String password = body.substring(i + strlen("Password="), body.indexOf("\r\n", i));

    if (system.getUserConfig()->web.password.equals(password)) {
      // Create a cookie
      char cookie_value[65];
      cookie_value[64] = '\0';
      esp_fill_random(cookie_value, 64);

      // Transform random values to valid alphanumeric chars
      for (size_t i = 0; i < 64; i++) {
        uint8_t b = cookie_value[i] % 62;
        if (b < 10) {
          cookie_value[i] = '0' + b;
        } else if (b < 36) {
          cookie_value[i] = 'A' + b - 10;
        } else {
          cookie_value[i] = 'a' + b - 36;
        }
      }

      // Send redirection request with cookie
      session_cookie cookie(millis(), cookie_value);
      connected_clients.insert({client.remoteIP(), cookie});
      client.println(STATUS_303_INFO + cookie.creationString());
    } else {
      // Invalid password
      String page = loadPage("/login.html");
      page.replace("$$STATUS$$", "<p class=\"warning\">Invalid password</p>");
      client.println(STATUS_200(client, header));
      client.println(page);
      client.println();
    }
  }
}

void WebTask::root_redirect(WiFiClient &client, webserver::Header_t &header, System &system) {
  if (isClientLoggedIn(client, header)) {
    client.println(STATUS_303_INFO);
  } else {
    client.println(STATUS_303_LOGIN);
  }
}

void WebTask::style_css(WiFiClient &client, webserver::Header_t &header, System &system) {
  client.println("HTTP/1.1 200 OK\r\nContent-type:text/css\r\nConnection: close\r\n");
  client.println(loadPage("/style.css"));
}

bool WebTask::isClientLoggedIn(const WiFiClient &client, const webserver::Header_t &header) const {
  std::map<uint32_t, struct session_cookie>::const_iterator it = connected_clients.find(client.remoteIP());
  if ((it != connected_clients.end()) && getSessionCookie(header).equals(it->second.value) && (millis() - it->second.timestamp < SESSION_LIFETIME)) {
    return true;
  }
  return false;
}

String WebTask::getSessionCookie(const webserver::Header_t &header) const {
  webserver::Header_t::const_iterator it_cookie = header.find("Cookie");
  if (it_cookie == header.cend()) {
    return String();
  }

  String cookieLine = it_cookie->second;
  cookieLine        = cookieLine.substring(cookieLine.indexOf("session=") + strlen("session="));
  String session;
  size_t j = 0;

  // Read the line while we have alphanum chars.
  while (true) {
    if (isalnum(cookieLine.charAt(j))) {
      session += cookieLine.charAt(j);
    } else {
      break;
    }
    j++;
  }

  return session;
}

String WebTask::session_cookie::creationString() {
  return String("Set-Cookie: session=" + value + "; Max-Age=900; HttpOnly; SameSite=Strict\r\n");
}

WebTask::session_cookie::session_cookie(unsigned long t, String val) : value(val), timestamp(t) {
}

String WebTask::STATUS_200(WiFiClient &client, const webserver::Header_t &header) {
  String response = "HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n";
  if (isClientLoggedIn(client, header)) {
    session_cookie cookie                                       = session_cookie(millis(), connected_clients.find(client.remoteIP())->second.value);
    connected_clients.find(client.remoteIP())->second.timestamp = millis();
    return response + cookie.creationString();
  } else {
    return response;
  }
}

String WebTask::sanitize(const String &string) {
  String output = "";

  for (size_t i = 0; i < string.length(); i++) {
    char c = string.charAt(i);

    if (c < ' ') {
      output += ("&#" + String((int)c) + ";");
    } else {
      switch (c) {
      case '<':
        output += "&lt;";
        break;
      case '>':
        output += "&gt;";
        break;
      case '&':
        output += "&amp;";
        break;
      case '"':
        output += "&quot;";
        break;
      case '\'':
        output += "&apos;";
        break;
      default:
        output += c;
      }
    }
  }

  return output;
}
