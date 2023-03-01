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

  String           loadPage(String file) const;
  String           readCRLFCRLF(httpd_req_t *req, size_t length) const;
  bool             isClientLoggedIn(httpd_req_t *req) const;
  static void      sanitize(String &string);
  ip_t             getReqIp(httpd_req_t *req) const;
  static esp_err_t redirectToLogin(httpd_req_t *req);
  static esp_err_t redirectToInfo(httpd_req_t *req);
  String           reqBumpCookie(httpd_req_t *req);

  /**
   * @brief   Read bytes from a request until terminator is met or length bytes are read.
   *
   *
   * @param[in] req The request the data should be read from
   *
   * @param[in] terminator The char until which to read data
   *
   * @param[in] buffer Buffer in which to store the data read (not including the terminator)
   *
   * @param[in] length Maximum number of bytes to read
   *
   * @return
   *  - Number of bytes read: The number of bytes read from the request (not including the terminator)
   *  - -1   : Connexion closed (no data read)
   *  - -408 : The read timed out
   *  - -500 : There was an error while reading from the request.
   */
  static int32_t readRequestUntil(httpd_req_t *req, const char terminator, uint8_t *buffer, uint16_t length);

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
