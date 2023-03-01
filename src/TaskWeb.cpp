#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_https_server.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <functional>
#include <logger.h>

#include "Task.h"
#include "TaskOTA.h"
#include "TaskWeb.h"
#include "project_configuration.h"

WebTask::WebTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system) : FreeRTOSTask(TASK_WEB, TaskWeb, priority, 8192, coreId, displayOnScreen), httpd_handle(NULL), _system(system) {
  start();
}

WebTask::~WebTask() {
}

void WebTask::worker() {

  SPIFFS.begin();

  // Wait for network
  while (!_system.isWifiOrEthConnected()) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  httpd_ssl_config_t https_config = HTTPD_SSL_CONFIG_DEFAULT();
  isServerStarted                 = false;
  _stateInfo                      = "Awaiting network";
  https_config.httpd.core_id      = 1;
  extern const unsigned char https_cert_start[] asm("_binary_ssl_servercert_pem_start");
  extern const unsigned char https_cert_end[] asm("_binary_ssl_servercert_pem_end");
  extern const unsigned char https_key_start[] asm("_binary_ssl_prvtkey_pem_start");
  extern const unsigned char https_key_end[] asm("_binary_ssl_prvtkey_pem_end");
  https_config.cacert_pem            = https_cert_start;
  https_config.cacert_len            = https_cert_end - https_cert_start;
  https_config.prvtkey_pem           = https_key_start;
  https_config.prvtkey_len           = https_key_end - https_key_start;
  https_config.httpd.global_user_ctx = this;
  // https_config.httpd.global_user_ctx_free_fn = free_context;
  https_config.httpd.stack_size = 25 * 1024;

  httpd_uri_t info_uri        = {.uri = "/info", .method = HTTP_GET, .handler = info_wrapper};
  httpd_uri_t enableota_uri   = {.uri = "/enableOTA", .method = HTTP_POST, .handler = enableota_wrapper};
  httpd_uri_t uploadfw_uri    = {.uri = "/uploadFW", .method = HTTP_POST, .handler = uploadfw_wrapper};
  httpd_uri_t login_uri_get   = {.uri = "/login", .method = HTTP_GET, .handler = login_wrapper};
  httpd_uri_t login_uri_post  = {.uri = "/login", .method = HTTP_POST, .handler = login_wrapper};
  httpd_uri_t style_css_uri   = {.uri = "/style.css", .method = HTTP_GET, .handler = style_css_wrapper};
  httpd_uri_t root_uri        = {.uri = "/", .method = HTTP_GET, .handler = root_wrapper};
  httpd_uri_t packets_log_uri = {.uri = "/packets.log", .method = HTTP_GET, .handler = download_packets_logs_wrapper};

  esp_err_t ret   = httpd_ssl_start(&httpd_handle, &https_config);
  isServerStarted = true;

  APP_LOGD(getName(), "httpd_ssl_start returned %d", ret);
  httpd_register_uri_handler(httpd_handle, &info_uri);
  httpd_register_uri_handler(httpd_handle, &enableota_uri);
  httpd_register_uri_handler(httpd_handle, &uploadfw_uri);
  httpd_register_uri_handler(httpd_handle, &login_uri_get);
  httpd_register_uri_handler(httpd_handle, &login_uri_post);
  httpd_register_uri_handler(httpd_handle, &style_css_uri);
  httpd_register_uri_handler(httpd_handle, &root_uri);
  httpd_register_uri_handler(httpd_handle, &packets_log_uri);

  // Task loop
  for (;;) {
    if (isServerStarted && !_system.isWifiOrEthConnected()) {
      isServerStarted = false;
      APP_LOGW(getName(), "Closed HTTP server because network connection was lost.");
      _stateInfo = "Awaiting network";
    }

    if (!isServerStarted && _system.isWifiOrEthConnected()) {
      isServerStarted = true;
      APP_LOGW(getName(), "Network connection recovered, http server restarted.");
      _stateInfo = "Online";
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

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

String WebTask::loadPage(String file) const {
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

/**
 * @brief   Read bytes from a request until a double CRLF is found.
 *
 *
 * @param[in] req The request the data should be read from
 *
 * @param[in] length Maximum number of bytes to read
 *
 * @return
 *  - String : String containing the data read (including CRLFCRLF)
 */
String WebTask::readCRLFCRLF(httpd_req_t *req, size_t length) const {
  String str = "";
  char   c;
  size_t i = 0;
  int    ret;

  while (i < length) {
    ret = httpd_req_recv(req, &c, 1);
    if (ret == 0) { // No more data to read
      return str;
    } else if (ret != 1) { // Error
      APP_LOGE(getName(), "readCRLFCRLF: httpd_req_recv returned -1");
      return "";
    }
    str += c;
    if (c == '\r') {
      // We might be at the start of a CRLFCRLF. Check for that
      char lfcrlf[4] = {0};

      ret = httpd_req_recv(req, lfcrlf, 3);
      if (ret >= 0 && ret < 3) {
        str += lfcrlf;
        return str;
      } else if (ret == 3 && (strcmp(lfcrlf, "\n\r\n") == 0)) {
        str += lfcrlf;
        return str;
      } else if (ret == 3) {
        str += lfcrlf;
      } else { // Error
        APP_LOGE(getName(), "readCRLFCRLF: httpd_req_recv returned %d", ret);
        return "";
      }
    }
    i++;
  }

  return str;
}

esp_err_t WebTask::info_page(httpd_req_t *req) {
  APP_LOGD(getName(), "info_handler");
  if (!isClientLoggedIn(req)) {
    return redirectToLogin(req);
  }

  String page = loadPage("/info.html");
  page.replace("$$CALLSIGN$$", _system.getUserConfig()->callsign);

  // Build $$TASKLIST$$ string
  String tasklist = "";
  for (FreeRTOSTask *task : _system.getTaskManager().getFreeRTOSTasks()) {
    switch (task->getTaskId()) {
    case TaskOta:
      switch (static_cast<OTATask *>(task)->getOTAStatus()) {
      case OTATask::Status::OTA_ForceDisabled:
        tasklist += String("<p class=\"task\">") + task->getName() + ": Disabled.</p>";
        break;
      case OTATask::Status::OTA_ForceEnabled:
        tasklist += String("<p class=\"task ok\">") + task->getName() + ": Enabled.</p>";
        break;
      case OTATask::Status::OTA_Disabled:
        tasklist += String("<p class=\"task warning\">") + task->getName() + ": Disabled. Use web interface to enable.</p>";
        break;
      case OTATask::Status::OTA_Enabled:
        tasklist += String("<p class=\"task ok\">") + task->getName() + ": Enabled for ";
        unsigned int seconds = static_cast<OTATask *>(task)->getTimeRemaining() / 1000;
        tasklist += String(seconds) + " more seconds.</p>";
        break;
      }
      break;
    case TaskWifi:
      tasklist += String("<p class=\"task ok\">") + task->getName() + ": connected to AP \"" + WiFi.SSID() + "\". RSSI is " + String(WiFi.RSSI()) +
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
      tasklist += String(task->getName()) + ": " + task->getStateInfo() + "</p>";
    }
  }

  page.replace("$$TASKLIST$$", tasklist);

  String logs = _system.getPacketLogger()->getTail();
  sanitize(logs);
  logs = "<tt>" + logs + "</tt>";
  page.replace("$$LOGSLIST$$", logs);
  page.trim();

  httpd_resp_set_status(req, "200 OK");
  String cookie_str = reqBumpCookie(req);
  if (!cookie_str.isEmpty()) {
    httpd_resp_set_hdr(req, "Set-Cookie", cookie_str.c_str());
  }

  return httpd_resp_send(req, page.c_str(), page.length());
}

esp_err_t WebTask::download_packets_logs(httpd_req_t *req) {
  if (!isClientLoggedIn(req)) {
    return redirectToLogin(req);
  }
  _system.getPacketLogger()->getFullLogs(req);

  return ESP_OK;
}

esp_err_t WebTask::enableota_page(httpd_req_t *req) {
  if (!isClientLoggedIn(req)) {
    return redirectToLogin(req);
  }

  // Load page and replace placeholder
  String page = loadPage("/enableOTA.html");
  for (FreeRTOSTask *it : _system.getTaskManager().getFreeRTOSTasks()) {
    if (it->getTaskId() == TaskOta) {
      static_cast<OTATask *>(it)->enableOTA(5 * 60 * 1000); // Enabling OTA for 5 minutes
      APP_LOGI(getName(), "User enabled OTA for 5 minutes via web interface");
      page.replace("$$STATUS$$", "<p style=\"text-align: center; color: white;\">OTA Enabled for 5 minutes.</p>");

      break;
    }
  }

  page.replace("$$STATUS$$", "<p style=\"text-align: center; color: red;\">There was an error with the OTA task.</p>");
  String cookieStr = reqBumpCookie(req);
  if (!cookieStr.isEmpty()) {
    httpd_resp_set_hdr(req, "Set-Cookie", cookieStr.c_str());
  }

  return httpd_resp_send(req, page.c_str(), page.length());
}

esp_err_t WebTask::uploadfw_page(httpd_req_t *req) {
  if (!isClientLoggedIn(req)) {
    return redirectToLogin(req);
  }
  // const String str_content_type = "Content-Type: multipart/form-data;";
  // const String str_content_length = "Content-Length: ";
  const String str_boundary = "boundary=";
  String       boundary_token;

  // Read content type to retrieve boundary token
  size_t content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type");
  if (content_type_len <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
  }
  char *content_type_str = new char[++content_type_len];
  if (content_type_str == NULL) {
    return httpd_resp_send_500(req);
  }
  httpd_req_get_hdr_value_str(req, "Content-Type", content_type_str, content_type_len);
  APP_LOGD(getName(), "Content-Type length is %d, string is \"%s\".", content_type_len, content_type_str);
  // Fetch boundary token
  String content_type = String(content_type_str, content_type_len);
  boundary_token      = "--" + content_type.substring(content_type.indexOf(str_boundary) + str_boundary.length());
  APP_LOGD(getName(), "The boundary to compare with is %s.", boundary_token.c_str());

  // Finished parsing header. Now parsing form data
  const size_t BUFFER_LENGTH                  = 511;
  uint8_t      full_buffer[BUFFER_LENGTH + 3] = {0}; // add two bytes to allow prepending data when LF was read, add 1 for terminator
  uint8_t     *read_buffer                    = full_buffer + 2;
  full_buffer[1]                              = '\n'; // This char will always be '\n'
  String    name;
  String    filename;
  esp_err_t esp_error = ESP_OK;

  // Read a line. It should be the first boundary of the form-data
  size_t boundary_len           = readRequestUntil(req, '\n', read_buffer, BUFFER_LENGTH);
  read_buffer[boundary_len - 1] = '\0'; // Replace '\r' by '\0'
  APP_LOGD("uploadFW", "boundary token = %s, buffer = %s.", boundary_token.c_str(), read_buffer);
  if (strcmp(boundary_token.c_str(), (const char *)read_buffer) == 0) { // We found a boundary token. It should be at the beginning of a form part
    while (esp_error == ESP_OK) {
      String headerStr = readCRLFCRLF(req, 2048); // Read the header that is just after the boundary
      // Determine field name
      APP_LOGD(getName(), "headerStr = \"%s\".", headerStr.c_str());
      if (headerStr.isEmpty()) { // Error while reading header
        esp_error = ESP_FAIL;
        break;
      } else if (headerStr.indexOf(boundary_token + "--\r\n") >= 0) { // End of multiform
        break;
      }
      name     = headerStr.substring(headerStr.indexOf("name=\"") + strlen("name=\""), headerStr.indexOf("\"; filename"));
      filename = headerStr.substring(headerStr.indexOf("filename=\"") + strlen("filename=\""), headerStr.indexOf("\"\r\n"));
      APP_LOGD(getName(), "Section with name=\"%s\" and filename = \"%s\".", name.c_str(), filename.c_str());

      if (name.equals("Firmware_File")) {
        // if no filename, we skip firmware upload
        if (filename.isEmpty()) {
          APP_LOGD(getName(), "No firmware file, skipping.");
          continue;
        }

        size_t                 file_size = 0;
        int                    len;
        const esp_partition_t *next_part = esp_ota_get_next_update_partition(NULL);
        esp_ota_handle_t       ota_handle;
        bool                   data_prepended = false;
        esp_error                             = esp_ota_begin(next_part, OTA_SIZE_UNKNOWN, &ota_handle);

        if (esp_error != ESP_OK) {
          APP_LOGW(getName(), "Error starting OTA.");
          break;
        }

        // While there is data available and we haven't read the boundary token
        while (esp_error == ESP_OK) {
          // Updating is gonna take some time so we need to reset watchdog.
          esp_task_wdt_reset();
          // Read from client and check if read was valid
          len = readRequestUntil(req, '\n', read_buffer, BUFFER_LENGTH);
          if (len < 0) {
            APP_LOGD(getName(), "readRequestUntil returned %d", len);
            esp_error = ESP_FAIL;
            break;
          } else if (len == 0) {
            read_buffer[0] = '\n';
            size_t tmp     = readRequestUntil(req, '\n', read_buffer + 1, BUFFER_LENGTH - 1);
            if (len < 0) {
              esp_error = ESP_FAIL;
              APP_LOGD(getName(), "readRequestUntil returned %d", tmp);
              break;
            }
            len = 1 + tmp;
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
              APP_LOGI(getName(), "len < 0");
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

        APP_LOGD(getName(), "Finished parsing binary file. Size is %d", file_size);

        if (esp_error == ESP_OK) {
          esp_error = esp_ota_end(ota_handle);
        } else {
          // esp_ota_abort(ota_handle);
          APP_LOGD(getName(), "Error while writing to ota. Code is %d", esp_error);
        }

        switch (esp_error) {
        case ESP_OK:
          APP_LOGD(getName(), "OTA Succeeded.");
          esp_ota_set_boot_partition(next_part);
          break;
        case ESP_ERR_NOT_FOUND:
          APP_LOGW(getName(), "ESP_ERR_NOT_FOUND.");
          break;
        case ESP_ERR_INVALID_ARG:
          APP_LOGW(getName(), "ESP_ERR_INVALID_ARG.");
          break;
        case ESP_ERR_OTA_VALIDATE_FAILED:
          APP_LOGW(getName(), "ESP_ERR_OTA_VALIDATE_FAILED.");
          break;
        case ESP_ERR_INVALID_STATE:
          APP_LOGW(getName(), "ESP_ERR_INVALID_STATE.");
          break;
        default:
          APP_LOGW(getName(), "ESP OTA unknown error...");
          break;
        }
      } else if (name.equals("SPIFFS_File")) {
        if (filename.isEmpty()) { // Do not update spiffs if we have no SPIFFS file
          APP_LOGD(getName(), "No SPIFFS file uploaded. Skipping.");
          continue;
        }

        size_t                   file_size = 0;
        int                      len;
        esp_partition_iterator_t spiffs_part_it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
        const esp_partition_t   *spiffs_part    = esp_partition_get(spiffs_part_it);
        esp_partition_iterator_release(spiffs_part_it);

        bool data_prepended = 0;

        esp_error = esp_partition_erase_range(spiffs_part, 0, spiffs_part->size);
        if (esp_error != ESP_OK) {
          APP_LOGW(getName(), "Impossible to erase SPIFFS partition...");
          break;
        }

        // While there is data available and we haven't read the boundary token
        while (esp_error == ESP_OK) {
          // Updating is gonna take some time so we need to reset watchdog.
          esp_task_wdt_reset();

          // Read from client and check if read was valid
          len = readRequestUntil(req, '\n', read_buffer, BUFFER_LENGTH);
          if (len < 0) {
            APP_LOGD(getName(), "readRequestUntil returned %d", len);
            esp_error = ESP_FAIL;
            break;
          } else if (len == 0) {
            read_buffer[0] = '\n';
            size_t tmp     = readRequestUntil(req, '\n', read_buffer + 1, BUFFER_LENGTH - 1);
            if (len < 0) {
              esp_error = ESP_FAIL;
              APP_LOGD(getName(), "readRequestUntil returned %d", tmp);
              break;
            }
            len = 1 + tmp;
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
          APP_LOGW(getName(), "Error %d while writing to spiffs partition.", esp_error);
          break;
        }
      }
    }
    /*else if ((boundary_token + "--").equals(buffer)) {
      // we found the end of form boundary
      system->log_debug(getName(), "Finished parsing full request.");
      break;
    }
    */
  } else {
    // No token where one is expected
    APP_LOGW(getName(), "Could not find firmware data.");
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
  }

  if (esp_error == ESP_OK) {
    APP_LOGW(getName(), "ESP OTA succeeded. Restarting in 2s.");
    httpd_resp_sendstr(req, "<!DOCTYPE html><html>"
                            "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                            "<link rel=\"icon\" href=\"data:,\">"
                            "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>"
                            "<body><h1>iGate Web Server</h1>"
                            "<p>OTA Update successful. Please give the device 30s to reboot.</p>"
                            "</body></html>\r\n");
    esp_restart();
  } else {
    APP_LOGW(getName(), "ESP OTA succeeded. Restarting in 2s.");
    httpd_resp_sendstr(req, "<!DOCTYPE html><html>"
                            "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                            "<link rel=\"icon\" href=\"data:,\">"
                            "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>"
                            "<body><h1>iGate Web Server</h1>"
                            "<p>Error during firmware upload. Please try again.</p>"
                            "</body></html>\r\n");
    return ESP_OK;
  }
}

esp_err_t WebTask::login_page(httpd_req_t *req) {
  // Check for POST/GET
  APP_LOGD(getName(), "login_handler");

  if (req->method == httpd_method_t::HTTP_GET) {
    if (isClientLoggedIn(req)) {
      return redirectToInfo(req);
    }

    String page = loadPage("/login.html");
    page.replace("$$STATUS$$", "");
    String cookieString = reqBumpCookie(req);
    if (!cookieString.isEmpty()) {
      httpd_resp_set_hdr(req, "Set-Cookie", cookieString.c_str());
    }
    return httpd_resp_send(req, page.c_str(), page.length());
  } else if (req->method == httpd_method_t::HTTP_POST) {
    // Read body content
    if (req->content_len > 256) {
      httpd_resp_set_status(req, "413 Payload Too Large");
      return httpd_resp_send(req, STATUS_413.c_str(), STATUS_413.length());
    }
    char *pcBody = new char[req->content_len];
    if (pcBody == NULL) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
    }
    httpd_req_recv(req, pcBody, req->content_len);
    String body(pcBody, req->content_len);
    delete pcBody;

    // Check password
    size_t i        = body.indexOf("Password=");
    String password = body.substring(i + strlen("Password="), body.indexOf("\r\n", i));

    if (_system.getUserConfig()->web.password.equals(password)) {
      APP_LOGD(getName(), "Password is correct !");
      // Create a cookie
      char cookie_value[65];
      cookie_value[64] = '\0';
      esp_fill_random(cookie_value, 64);

      // Transform random values to valid alphanumeric chars
      for (size_t j = 0; j < 64; j++) {
        uint8_t b = cookie_value[j] % 62;
        if (b < 10) {
          cookie_value[j] = '0' + b;
        } else if (b < 36) {
          cookie_value[j] = 'A' + b - 10;
        } else {
          cookie_value[j] = 'a' + b - 36;
        }
      }

      ip_t clientIP = getReqIp(req);
      // Send redirection request with cookie
      session_cookie cookie(millis(), cookie_value);
      connected_clients.erase(clientIP);
      connected_clients.insert({clientIP, cookie});

      httpd_resp_set_status(req, "303 See Other");
      String    strCookieCreation = cookie.creationString();
      esp_err_t ret;

      ret = httpd_resp_set_hdr(req, "Set-Cookie", strCookieCreation.c_str());
      APP_LOGD(getName(), "add header Set-Cookie returned %d", ret);

      char location[] = "/info";
      ret             = httpd_resp_set_hdr(req, "Location", location);
      APP_LOGD(getName(), "add header location returned %d", ret);
      ret = httpd_resp_send(req, NULL, 0);
      APP_LOGD(getName(), "send returned %d", ret);
      return ret;
    } else {
      // Invalid password
      APP_LOGD(getName(), "Password is incorrect");
      String page = loadPage("/login.html");
      page.replace("$$STATUS$$", "<p class=\"warning\">Invalid password</p>");
      return httpd_resp_send(req, page.c_str(), page.length());
    }
  }

  return ESP_OK;
}

esp_err_t WebTask::root_redirect(httpd_req_t *req) {
  APP_LOGD(getName(), "root handler");
  if (isClientLoggedIn(req)) {
    return redirectToInfo(req);
  } else {
    return redirectToLogin(req);
  }
}

esp_err_t WebTask::style_css(httpd_req_t *req) {
  APP_LOGD(getName(), "style_css_handler");
  httpd_resp_set_type(req, "text/css");
  String page = loadPage("/style.css");
  httpd_resp_send(req, page.c_str(), page.length());
  return ESP_OK;
}

bool WebTask::isClientLoggedIn(httpd_req_t *req) const {
  ip_t   clientIp           = getReqIp(req);
  size_t session_cookie_len = 65;
  char  *session_cookie_str = new char[65];
  // Get required str length to store cookie
  esp_err_t ret = httpd_req_get_cookie_val(req, "session", session_cookie_str, &session_cookie_len);

  if (ret == ESP_ERR_NOT_FOUND) {
    // Client does not contain cookie
    APP_LOGD(getName(), "Could not find cookie \"session\"");
    return false;
  }
  if (ret == ESP_ERR_HTTPD_RESULT_TRUNC) {
    APP_LOGD(getName(), "httpd_req_get_cookie_val returned RESULT_TRUNC. session_cookie_len=%d.", session_cookie_len);
    delete session_cookie_str;
    session_cookie_str = new char[session_cookie_len];
  } else {
    APP_LOGD(getName(), "httpd_req_get_cookie_val returned %d. session_cookie_len=%d.", ret, session_cookie_len);
  }
  // Allocate memory and read cookie
  if (session_cookie_str == NULL) {
    APP_LOGD(getName(), "Could not allocate memory for cookie value");
    return false;
  }
  ret = httpd_req_get_cookie_val(req, "session", session_cookie_str, &session_cookie_len);
  if (ret != ESP_OK) {
    APP_LOGD(getName(), "Could not get cookie (returned %d)", ret);
    delete session_cookie_str;
    return false;
  }

  // Get corresponding session cookie in memory
  std::map<ip_t, session_cookie>::const_iterator it = connected_clients.find(clientIp);

  // Check that the stored cookie match the given one, and that the cookie did not timeout.
  if ((it != connected_clients.cend()) && it->second.value.equals(session_cookie_str) && (millis() - it->second.timestamp < SESSION_LIFETIME)) {
    delete session_cookie_str;
    return true;
  }

  APP_LOGD(getName(), "Cookie does not match.");

  delete session_cookie_str;
  return false;
}

String WebTask::session_cookie::creationString() const {
  return String("session=" + value + "; Max-Age=900; HttpOnly; SameSite=Strict");
}

WebTask::session_cookie::session_cookie(unsigned long t, String val) : value(val), timestamp(t) {
}

void WebTask::sanitize(String &string) {
  string.replace("&", "&amp;");
  string.replace("<", "&lt;");
  string.replace(">", "&gt;");

  string.replace("\"", "&quot;");
  string.replace("'", "&apos;");
  string.replace("\n", "<br>");
  string.replace("\t", "&emsp;");
}

WebTask::ip_t WebTask::getReqIp(httpd_req_t *req) const {
  ip_t ip     = {0};
  int  sockfd = httpd_req_to_sockfd(req);

  if (sockfd == -1) {
    APP_LOGD("GetIP", "Could not get sockfd from http request");
    return ip;
  }

  struct sockaddr_in6 client_name;
  socklen_t           name_len = sizeof(client_name);

  if (getpeername(sockfd, (struct sockaddr *)&client_name, &name_len) == 0) {
    ip[0] = client_name.sin6_addr.un.u32_addr[0];
    ip[1] = client_name.sin6_addr.un.u32_addr[1];
    ip[2] = client_name.sin6_addr.un.u32_addr[2];
    ip[3] = client_name.sin6_addr.un.u32_addr[3];
  }

  return ip;
}

esp_err_t WebTask::redirectToLogin(httpd_req_t *req) {
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/login");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebTask::redirectToInfo(httpd_req_t *req) {
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/info");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

String WebTask::reqBumpCookie(httpd_req_t *req) {
  ip_t requestIp = getReqIp(req);

  std::map<ip_t, session_cookie>::iterator it = connected_clients.find(requestIp);
  if (it != connected_clients.end()) {
    it->second.timestamp = millis();
    return it->second.creationString();
  }

  return "";
}

int32_t WebTask::readRequestUntil(httpd_req_t *req, const char terminator, uint8_t *buffer, uint16_t length) {
  if (length < 1) {
    return 0;
  }

  esp_err_t ret = ESP_OK;
  char      c;

  size_t i = 0;
  while (i < length) {
    ret = httpd_req_recv(req, &c, 1);
    if (ret == 0) {
      if (i == 0) {
        return -1;
      } else {
        break;
      }
    } else if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      return -408;
    } else if (ret == 1 && c == terminator) {
      break;
    } else if (ret == 1) {
      *buffer++ = c;
      i++;
    } else {
      return -500;
    }
  }

  return i;
}
