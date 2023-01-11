#include <WiFi.h>
#include <logger.h>

#include "Task.h"
#include "TaskEth.h"
#include "TaskWifi.h"
#include "project_configuration.h"

WifiTask::WifiTask(UBaseType_t priority, BaseType_t coreId, System &system) : FreeRTOSTask(TASK_WIFI, TaskWifi, priority, 2048 + 4096, coreId) {
  _system = &system;
  start();
}

void WifiTask::worker() {
  WiFi.persistent(false);
  if (_system->getUserConfig()->network.hostname.overwrite) {
    WiFi.setHostname(_system->getUserConfig()->network.hostname.name.c_str());
  } else {
    WiFi.setHostname(_system->getUserConfig()->callsign.c_str());
  }
  if (!_system->getUserConfig()->network.DHCP) {
    WiFi.config(_system->getUserConfig()->network.static_.ip, _system->getUserConfig()->network.static_.gateway, _system->getUserConfig()->network.static_.subnet, _system->getUserConfig()->network.static_.dns1, _system->getUserConfig()->network.static_.dns2);
  }

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  WiFi.onEvent(NetworkEvent);

  for (Configuration::Wifi::AP ap : _system->getUserConfig()->wifi.APs) {
    logger.debug(getName(), "Looking for AP: %s", ap.SSID.c_str());
    _wiFiMulti.addAP(ap.SSID.c_str(), ap.password.c_str());
  }

  for (;;) {
    uint8_t wifi_status = _wiFiMulti.run();
    if (wifi_status == WL_CONNECTED) {
      if (_oldWifiStatus != WL_CONNECTED) {
        _oldWifiStatus = WL_CONNECTED;
        _system->connectedViaWifi(true);
        _stateInfo = String("IP .") + String(WiFi.localIP()[3]) + String(" @ ") + String(WiFi.RSSI()) + String("dBm");
      }
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {
      if (_oldWifiStatus != wifi_status) {
        _oldWifiStatus = wifi_status;
        _state         = Error;
        _stateInfo     = "Not connected";
        _system->connectedViaWifi(false);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}
