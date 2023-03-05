#ifndef TASK_WEB_H_
#define TASK_WEB_H_

#include <TaskManager.h>
#include <WiFiMulti.h>
#include <esp_https_server.h>
#include <map>

class WebTask : public FreeRTOSTask {
public:
  WebTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system);
  virtual ~WebTask();

  void worker() override;

private:
  struct session_cookie {
    session_cookie(unsigned long timestamp, String value);
    String        value;     // Session id (64) + terminator
    unsigned long timestamp; // Timestamp (in ms) when the cookie was last created / refreshed

    String creationString() const;
  };

  typedef std::array<uint32_t, 4> ip_t;

  httpd_handle_t     httpd_handle;
  System            &_system;
  bool               isServerStarted  = false;
  const unsigned int SESSION_LIFETIME = 900000; // user session lifetime in ms (900 000 is 15 minutes)
  const String       STATUS_413       = String("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                                                           "<link rel=\"icon\" href=\"data:,\"><body><h1>413 Payload Too Large</h1></body></html>\r\n");

  std::map<ip_t, struct session_cookie> connected_clients; // Key is ip address, value is cookie

  /**
   * @brief     Loads a page from the SPIFFS partition
   *
   * @param[in] file A String containing the name of the file to read
   *
   * @return    String: A String containing the page read.
   */
  String loadPage(String file) const;

  /**
   * @brief     Reads from a request until either length chars have been read or until the substring "\r\n\r\n" is found whichever comes first.
   *
   * @param[in] req The request to read from.
   *
   * @param[in] length The maximum number of chars to read.
   *
   * @return    String: a String containing the data read, including \r\n\r\n.
   *
   * @note      If the character at the length position is "\\r" then three more chars will be read thus the resulting string might be up to length+3 chars long.
   */
  String readCRLFCRLF(httpd_req_t *req, size_t length) const;

  /**
   * @brief       Checks if the client of the given request is logged in
   *
   * @param[in]   req The request whose login status must be checked
   *
   * @return      True if the client is logged in, false otherwise.
   */
  bool isClientLoggedIn(httpd_req_t *req) const;

  /**
   * @brief         Sanitizes a string for displaying in an html document
   *
   * The function will escape with the corresponding HTML codes the characters "&", "<", ">", "\\", "'", "\\n", "\\t".
   * The character "\\t" will be replaced by &emsp
   *
   * @param[in,out] string, the string to sanitize
   */
  static void sanitize(String &string);

  /**
   * @brief     Returns the IP address corresponding to the given request.
   *
   * @param[in] req The request whose client's ip should be retrieved.
   *
   * @return    ip_t: the ip address of the client as an array of four uint32_t
   */
  ip_t getReqIp(httpd_req_t *req) const;

  /**
   * @brief     Redirects the client corresponding to the given request to the login page.
   *
   * @param[in] req The request of the client to redirect.
   *
   * @return    esp_err_t : the result of the httpd_resp_send function.
   *
   * @note      After this function, the request is sent and no more data can be sent to the client.
   */
  static esp_err_t redirectToLogin(httpd_req_t *req);

  /**
   * @brief     Redirects the client corresponding to the given request to the info page.
   *
   * @param[in] req The request of the client to redirect.
   *
   * @return    esp_err_t : the result of the httpd_resp_send function.
   *
   * @note      After this function, the request is sent and no more data can be sent to the client.
   */
  static esp_err_t redirectToInfo(httpd_req_t *req);

  /**
   * @brief     Updates the timestamp of the cookie corresponding to the given request.
   *
   * @param[in] req The request whose corresponding cookie should be bumped.
   *
   * @return    String containing the HTTP creation string (for the Set-Cookie header).
   */
  String reqBumpCookie(httpd_req_t *req);

  /**
   * @brief     Read bytes from a request until terminator is met or length bytes are read.
   *
   * @param[in] req The request the data should be read from.
   *
   * @param[in] terminator The char until which to read data.
   *
   * @param[in] buffer Buffer in which to store the data read (not including the terminator).
   *
   * @param[in,out] length Size of the buffer passed as argument. When the function returns, it will contain the number of bytes read.
   *
   * @return
   *  2 : finished, CRLFCRLF found
   *  1 : finished, no CRLFCRLF found
   *  other : an error returned by httpd_req_recv;
   */
  static int32_t readRequestUntil(httpd_req_t *req, const char terminator, uint8_t *buffer, size_t *length);

  /**
   * @brief Read bytes from a request until either the buffer is full or a CRLFCRLF token is reached.
   *
   * If no CRLFCRLF token is found, the buffer won't contain more than length-2 bytes.
   *
   * @param[in] req The request the data should be read from.
   *
   * @param[out] buffer The buffer in which the data read will be stored
   *
   * @param[in,out] length Size of the buffer passed as argument. When the function returns, it will contain the number of bytes read.
   *
   * @return
   *  2 : finished, CRLFCRLF found
   *  1 : finished, no CRLFCRLF found
   *  other : an error returned by httpd_req_recv;
   *
   * @note If a CRLFCRLF is read, it will not be stored in the buffer.
   *
   */
  static int32_t readRequestUntilCRLFCRLF(httpd_req_t *req, uint8_t *buffer, size_t *length);

  /**
   * @brief Read bytes from a request until either the buffer is full or a CRLF token is reached.
   *
   * @param[in] req The request the data should be read from.
   *
   * @param[out] buffer The buffer in which the data read will be stored
   *
   * @param[in,out] length Size of the buffer passed as argument. When the function returns, it will contain the number of bytes read.
   *
   * @return
   *  2 : finished, CRLF found
   *  1 : finished, no CRLF found
   *  other : an error returned by httpd_req_recv;
   *
   * @note If a CRLF is read, it will not be stored in the buffer.
   */
  static int32_t readRequestUntilCRLF(httpd_req_t *req, uint8_t *buffer, size_t *length);

  esp_err_t parseAndWriteFirmware(httpd_req_t *req, String boundary_token) const;
  esp_err_t parseAndWriteSPIFFS(httpd_req_t *req, String boundary_token) const;

  esp_err_t info_page(httpd_req_t *req);
  esp_err_t enableota_page(httpd_req_t *req);
  esp_err_t uploadfw_page(httpd_req_t *req);
  esp_err_t login_page(httpd_req_t *req);
  esp_err_t style_css(httpd_req_t *req);
  esp_err_t root_redirect(httpd_req_t *req);
  esp_err_t download_packets_logs(httpd_req_t *req);

  static esp_err_t info_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->info_page(req);
  }

  static esp_err_t enableota_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->enableota_page(req);
  }

  static esp_err_t uploadfw_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->uploadfw_page(req);
  }

  static esp_err_t login_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->login_page(req);
  }

  static esp_err_t style_css_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->style_css(req);
  }

  static esp_err_t root_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->root_redirect(req);
  }

  static esp_err_t download_packets_logs_wrapper(httpd_req_t *req) {
    WebTask *ctx = static_cast<WebTask *>(httpd_get_global_user_ctx(req->handle));
    return ctx->download_packets_logs(req);
  }
};

#endif
