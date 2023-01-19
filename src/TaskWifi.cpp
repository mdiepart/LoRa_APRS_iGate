#include <WiFi.h>
#include <logger.h>

#include "Task.h"
#include "TaskEth.h"
#include "TaskWifi.h"
#include "project_configuration.h"

WifiTask::WifiTask(UBaseType_t priority, BaseType_t coreId, System &system) : FreeRTOSTask(TASK_WIFI, TaskWifi, priority, 2048 + 4096, coreId), _system(system) {
  start();
}

void WifiTask::worker() {
  WiFi.persistent(false);
  if (_system.getUserConfig()->network.hostname.overwrite) {
    WiFi.setHostname(_system.getUserConfig()->network.hostname.name.c_str());
  } else {
    WiFi.setHostname(_system.getUserConfig()->callsign.c_str());
  }
  if (!_system.getUserConfig()->network.DHCP) {
    WiFi.config(_system.getUserConfig()->network.static_.ip, _system.getUserConfig()->network.static_.gateway, _system.getUserConfig()->network.static_.subnet, _system.getUserConfig()->network.static_.dns1, _system.getUserConfig()->network.static_.dns2);
  }

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  WiFi.onEvent(NetworkEvent);

  for (Configuration::Wifi::AP ap : _system.getUserConfig()->wifi.APs) {
    APP_LOGD(getName(), "Looking for AP: %s", ap.SSID.c_str());
    _wiFiMulti.addAP(ap.SSID.c_str(), ap.password.c_str());
  }

  uint32_t timeDisconnected = millis();

  for (;;) {
    uint8_t wifi_status = _wiFiMulti.run();
    if (wifi_status == WL_CONNECTED) {
      if (_oldWifiStatus != WL_CONNECTED) {
        _oldWifiStatus = WL_CONNECTED;
        _system.connectedViaWifi(true);
        _stateInfo = String("IP .") + String(WiFi.localIP()[3]) + String(" @ ") + String(WiFi.RSSI()) + String("dBm");
      }
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {
      if (_oldWifiStatus == WL_CONNECTED) {
        timeDisconnected = millis();
      }
      if (_oldWifiStatus != wifi_status) {
        _oldWifiStatus = wifi_status;
        _state         = Error;
        _stateInfo     = "Not connected";
        _system.connectedViaWifi(false);
        switch (wifi_status) {
        case WL_DISCONNECTED:
          APP_LOGD(getName(), "WiFi disconnected.");
          break;
        case WL_CONNECTION_LOST:
          APP_LOGD(getName(), "Connection lost.");
          break;
        case WL_IDLE_STATUS:
          APP_LOGD(getName(), "Idle.");
          break;
        case WL_NO_SSID_AVAIL:
          APP_LOGD(getName(), "No SSID available.");
          break;
        case WL_SCAN_COMPLETED:
          APP_LOGD(getName(), "Scan completed.");
          break;
        case WL_CONNECT_FAILED:
          APP_LOGD(getName(), "Connect failed");
          break;
        default:
          APP_LOGD(getName(), "Status %d.", wifi_status);
        }
        /* If we are disconnected for more than 1 minute, restart the module */
        if (!_system.isWifiOrEthConnected() && (millis() - timeDisconnected > 60000)) {
          ESP.restart();
        }
      }

      vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
  }
}
