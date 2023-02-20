#include <ETH.h>
#include <WiFi.h>
#include <logger.h>

#include "System.h"
#include "Task.h"
#include "TaskEth.h"
#include "project_configuration.h"

#define WIFI_EVENT "NetworkEvent"

volatile bool eth_connected = false;

void NetworkEvent(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_STA_START:
    APP_ISR_LOGI(WIFI_EVENT, "WiFi Started");
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    APP_ISR_LOGI(WIFI_EVENT, "WiFi Connected");
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    APP_ISR_LOGI(WIFI_EVENT, "WiFi MAC: %s", WiFi.macAddress().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "WiFi IPv4: %s", WiFi.localIP().toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "WiFi Gateway: %s", WiFi.gatewayIP().toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "WiFi DNS1: %s", WiFi.dnsIP().toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "WiFi DNS2: %s", WiFi.dnsIP(1).toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "WiFi Hostname: %s", WiFi.getHostname());
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    APP_ISR_LOGI(WIFI_EVENT, "WiFi Disconnected");
    break;
  case SYSTEM_EVENT_STA_STOP:
    APP_ISR_LOGI(WIFI_EVENT, "WiFi Stopped");
    break;
  case SYSTEM_EVENT_ETH_START:
    APP_ISR_LOGI(WIFI_EVENT, "ETH Started");
    break;
  case SYSTEM_EVENT_ETH_CONNECTED:
    APP_ISR_LOGI(WIFI_EVENT, "ETH Connected");
    break;
  case SYSTEM_EVENT_ETH_GOT_IP:
    APP_ISR_LOGI(WIFI_EVENT, "ETH MAC: %s", ETH.macAddress().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "ETH IPv4: %s", ETH.localIP().toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "ETH Gateway: %s", ETH.gatewayIP().toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "ETH DNS1: %s", ETH.dnsIP().toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "ETH DNS2: %s", ETH.dnsIP(1).toString().c_str());
    APP_ISR_LOGI(WIFI_EVENT, "ETH Hostname: %s", ETH.getHostname());
    if (ETH.fullDuplex()) {
      APP_ISR_LOGI(WIFI_EVENT, "ETH FULL_DUPLEX");
    }
    APP_ISR_LOGI(WIFI_EVENT, "ETH Speed: %dMbps", ETH.linkSpeed());
    eth_connected = true;
    break;
  case SYSTEM_EVENT_ETH_DISCONNECTED:
    APP_ISR_LOGW(WIFI_EVENT, "ETH Disconnected");
    eth_connected = false;
    break;
  case SYSTEM_EVENT_ETH_STOP:
    APP_ISR_LOGW(WIFI_EVENT, "ETH Stopped");
    eth_connected = false;
    break;
  default:
    break;
  }
}

EthTask::EthTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system) : FreeRTOSTask(TASK_ETH, TaskEth, priority, 2048, coreId, displayOnScreen), _system(system) {
  start();
}

EthTask::~EthTask() {
}

void EthTask::worker() {
  WiFi.onEvent(NetworkEvent);

  constexpr uint8_t          ETH_NRST      = 5;
  constexpr uint8_t          ETH_ADDR      = 0;
  constexpr int              ETH_POWER_PIN = -1;
  constexpr int              ETH_MDC_PIN   = 23;
  constexpr int              ETH_MDIO_PIN  = 18;
  constexpr eth_phy_type_t   ETH_TYPE      = ETH_PHY_LAN8720;
  constexpr eth_clock_mode_t ETH_CLK       = ETH_CLOCK_GPIO17_OUT; // TTGO PoE V1.0
  // constexpr eth_clock_mode_t ETH_CLK       = ETH_CLOCK_GPIO0_OUT;  // TTGO PoE V1.2

  pinMode(ETH_NRST, OUTPUT);
  digitalWrite(ETH_NRST, 0);
  delay(200);
  digitalWrite(ETH_NRST, 1);
  delay(200);
  digitalWrite(ETH_NRST, 0);
  delay(200);
  digitalWrite(ETH_NRST, 1);

  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK);

  if (!_system.getUserConfig()->network.DHCP) {
    ETH.config(_system.getUserConfig()->network.static_.ip, _system.getUserConfig()->network.static_.gateway, _system.getUserConfig()->network.static_.subnet, _system.getUserConfig()->network.static_.dns1, _system.getUserConfig()->network.static_.dns2);
  }
  if (_system.getUserConfig()->network.hostname.overwrite) {
    ETH.setHostname(_system.getUserConfig()->network.hostname.name.c_str());
  } else {
    ETH.setHostname(_system.getUserConfig()->callsign.c_str());
  }

  for (;;) {
    if (!eth_connected) {
      _system.connectedViaEth(false);
      _stateInfo = "Ethernet not connected";
      _state     = Error;
    } else {
      _system.connectedViaEth(true);
      _stateInfo = ETH.localIP().toString();
      _state     = Okay;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
