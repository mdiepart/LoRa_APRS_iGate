#include <APRS-IS.h>
#include <BoardFinder.h>
#include <System.h>
#include <TaskManager.h>
#include <esp_sntp.h>
#include <esp_task_wdt.h>
#include <logger.h>
#include <power_management.h>

#include "TaskAprsIs.h"
#include "TaskBeacon.h"
#include "TaskDisplay.h"
#include "TaskEth.h"
#include "TaskFTP.h"
#include "TaskMQTT.h"
#include "TaskOTA.h"
#include "TaskPacketLogger.h"
#include "TaskRadiolib.h"
#include "TaskRouter.h"
#include "TaskWeb.h"
#include "TaskWifi.h"
#include "project_configuration.h"

#define VERSION     "23.09.0"
#define MODULE_NAME "Main"

QueueHandle_t toAprsIs;
QueueHandle_t fromModem;
QueueHandle_t toModem;
QueueHandle_t toMQTT;
QueueHandle_t toPacketLogger;
QueueHandle_t toDisplay;

System        LoRaSystem;
Configuration userConfig;

DisplayTask      *displayTask;
RadiolibTask     *modemTask;
EthTask          *ethTask;
WifiTask         *wifiTask;
OTATask          *otaTask;
FTPTask          *ftpTask;
MQTTTask         *mqttTask;
WebTask          *webTask;
AprsIsTask       *aprsIsTask;
RouterTask       *routerTask;
BeaconTask       *beaconTask;
PacketLoggerTask *packetLoggerTask;

void sntp_sync_callback_fn(timeval *timeVal);

void setup() {
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);
  Serial.begin(115200);
  delay(500);
  APP_LOGI(MODULE_NAME, "LoRa APRS iGate by OE5BPA (Peter Buchegger)");
  APP_LOGI(MODULE_NAME, "Version: %s", VERSION);

  switch (esp_reset_reason()) {
  case ESP_RST_POWERON:
    APP_LOGD(MODULE_NAME, "Module started.");
    break;
  case ESP_RST_EXT:
    APP_LOGD(MODULE_NAME, "Module reset by external pin.");
    break;
  case ESP_RST_SW:
    APP_LOGD(MODULE_NAME, "Module reset following software call.");
    break;
  case ESP_RST_PANIC:
    APP_LOGD(MODULE_NAME, "Module reset because of panic.");
    break;
  case ESP_RST_INT_WDT:
    APP_LOGD(MODULE_NAME, "Module reset due to interrupt watchdog.");
    break;
  case ESP_RST_TASK_WDT:
    APP_LOGD(MODULE_NAME, "Module reset because of task watchdog timer.");
    break;
  case ESP_RST_WDT:
    APP_LOGD(MODULE_NAME, "Module reset due to other watchdogs.");
    break;
  case ESP_RST_DEEPSLEEP:
    APP_LOGD(MODULE_NAME, "Module reset after exiting deepsleep.");
    break;
  case ESP_RST_BROWNOUT:
    APP_LOGD(MODULE_NAME, "Module reset by brownout reset.");
    break;
  case ESP_RST_SDIO:
    APP_LOGD(MODULE_NAME, "Module reset by SDIO.");
    break;
  default:
    APP_LOGD(MODULE_NAME, "Module reset for reason %d.", (int)esp_reset_reason());
    break;
  }

  std::list<BoardConfig const *> boardConfigs;
  boardConfigs.push_back(&TTGO_LORA32_V1);
  boardConfigs.push_back(&TTGO_LORA32_V2);
  boardConfigs.push_back(&TTGO_T_Beam_V0_7);
  boardConfigs.push_back(&TTGO_T_Beam_V1_0);
  boardConfigs.push_back(&ETH_BOARD);
  boardConfigs.push_back(&TRACKERD);
  boardConfigs.push_back(&HELTEC_WIFI_LORA_32_V1);
  boardConfigs.push_back(&HELTEC_WIFI_LORA_32_V2);
  boardConfigs.push_back(&GUALTHERIUS_LORAHAM_v100);
  boardConfigs.push_back(&GUALTHERIUS_LORAHAM_v106);

  ProjectConfigurationManagement confmg;
  confmg.readConfiguration(userConfig);

  BoardFinder        finder(boardConfigs);
  BoardConfig const *boardConfig = finder.getBoardConfig(userConfig.board);
  if (!boardConfig) {
    boardConfig = finder.searchBoardConfig();
    if (!boardConfig) {
      APP_LOGE(MODULE_NAME, "Board config not set and search failed!");
      while (true)
        ;
    } else {
      userConfig.board = boardConfig->Name;
      confmg.writeConfiguration(userConfig);
      APP_LOGI(MODULE_NAME, "will restart board now!");
      ESP.restart();
    }
  }

  APP_LOGI(MODULE_NAME, "Board %s loaded.", boardConfig->Name.c_str());

  if (boardConfig->Type == eTTGO_T_Beam_V1_0) {
    Wire.begin(boardConfig->OledSda, boardConfig->OledScl);
    PowerManagement powerManagement;
    if (!powerManagement.begin(Wire)) {
      APP_LOGI(MODULE_NAME, "AXP192 init done!");
    } else {
      APP_LOGE(MODULE_NAME, "AXP192 init failed!");
    }
    powerManagement.activateLoRa();
    powerManagement.activateOLED();
    if (userConfig.beacon.use_gps) {
      powerManagement.activateGPS();
    } else {
      powerManagement.deactivateGPS();
    }
  }

  toAprsIs       = xQueueCreate(10, sizeof(APRSMessage *));
  toModem        = xQueueCreate(10, sizeof(APRSMessage *));
  fromModem      = xQueueCreate(10, sizeof(APRSMessage *));
  toMQTT         = xQueueCreate(10, sizeof(APRSMessage *));
  toPacketLogger = xQueueCreate(10, sizeof(logEntry));
  toDisplay      = xQueueCreate(10, sizeof(TextFrame *));

  LoRaSystem.setBoardConfig(boardConfig);
  LoRaSystem.setUserConfig(&userConfig);
  displayTask = new DisplayTask(1, 0, true, LoRaSystem, toDisplay, VERSION);
  LoRaSystem.getTaskManager().addFreeRTOSTask(displayTask);

  modemTask = new RadiolibTask(5, 0, false, LoRaSystem, fromModem, toModem, toPacketLogger, toDisplay);
  LoRaSystem.getTaskManager().addFreeRTOSTask(modemTask);
  routerTask = new RouterTask(4, 0, false, LoRaSystem, fromModem, toModem, toAprsIs, toMQTT);
  LoRaSystem.getTaskManager().addFreeRTOSTask(routerTask);
  beaconTask = new BeaconTask(3, 0, true, LoRaSystem, toModem, toAprsIs, toDisplay);
  LoRaSystem.getTaskManager().addFreeRTOSTask(beaconTask);

  bool tcpip = false;

  if (userConfig.wifi.active) {
    wifiTask = new WifiTask(6, 0, true, LoRaSystem);
    LoRaSystem.getTaskManager().addFreeRTOSTask(wifiTask);
    tcpip = true;
  }
  if (boardConfig->Type == eETH_BOARD) {
    ethTask = new EthTask(6, 0, true, LoRaSystem);
    LoRaSystem.getTaskManager().addFreeRTOSTask(ethTask);
    tcpip = true;
  }

  if (tcpip) {
    if (LoRaSystem.getUserConfig()->ota.active) {
      otaTask = new OTATask(4, 1, true, LoRaSystem);
      LoRaSystem.getTaskManager().addFreeRTOSTask(otaTask);
    }

    if (userConfig.ftp.active) {
      ftpTask = new FTPTask(1, 1, false, LoRaSystem);
      LoRaSystem.getTaskManager().addFreeRTOSTask(ftpTask);
    }

    if (userConfig.aprs_is.active) {
      aprsIsTask = new AprsIsTask(4, 0, true, LoRaSystem, toAprsIs);
      LoRaSystem.getTaskManager().addFreeRTOSTask(aprsIsTask);
    }

    if (userConfig.mqtt.active) {
      mqttTask = new MQTTTask(4, 0, true, LoRaSystem, toMQTT);
      LoRaSystem.getTaskManager().addFreeRTOSTask(mqttTask);
    }

    if (userConfig.web.active) {
      webTask = new WebTask(3, 1, true, LoRaSystem);
      LoRaSystem.getTaskManager().addFreeRTOSTask(webTask);
    }
  }

  if (userConfig.packetLogger.active) {
    packetLoggerTask = new PacketLoggerTask(2, 1, true, LoRaSystem, "packets.log", toPacketLogger);
    LoRaSystem.getTaskManager().addFreeRTOSTask(packetLoggerTask);
    LoRaSystem.setPacketLogger(packetLoggerTask);
  }

  esp_task_wdt_reset();

  if (tcpip) {
    setenv("TZ", LoRaSystem.getUserConfig()->timezone.c_str(), 1);
    tzset();
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_interval(3600 * 1000); // One hour
    sntp_setservername(0, LoRaSystem.getUserConfig()->ntpServer.c_str());
    sntp_set_time_sync_notification_cb(sntp_sync_callback_fn);
    sntp_init();
  }

  if (userConfig.callsign == "NOCALL-10") {
    APP_LOGE(MODULE_NAME, "You have to change your settings in 'data/is-cfg.json' and upload it via 'Upload File System image'!");
    // LoRaSystem.getDisplay().showStatusScreen("ERROR", "You have to change your settings in 'data/is-cfg.json' and upload it via \"Upload File System image\"!");
    while (true)
      ;
  }
  if ((!userConfig.aprs_is.active) && !(userConfig.digi.active)) {
    APP_LOGE(MODULE_NAME, "No mode selected (iGate or Digi)! You have to activate one of iGate or Digi.");
    // LoRaSystem.getDisplay().showStatusScreen("ERROR", "No mode selected (iGate or Digi)! You have to activate one of iGate or Digi.");
    while (true)
      ;
  }

  if (userConfig.display.overwritePin != 0) {
    pinMode(userConfig.display.overwritePin, INPUT);
    pinMode(userConfig.display.overwritePin, INPUT_PULLUP);
  }

  APP_LOGI(MODULE_NAME, "setup done...");
}

volatile bool syslogSet = false;

void loop() {
  esp_task_wdt_reset();

  if (LoRaSystem.isWifiOrEthConnected() && LoRaSystem.getUserConfig()->syslog.active && !syslogSet) {
    logger.setSyslogServer(LoRaSystem.getUserConfig()->syslog.server, LoRaSystem.getUserConfig()->syslog.port, LoRaSystem.getUserConfig()->callsign);
    APP_LOGI(MODULE_NAME, "System connected after a restart to the network, syslog server set");
    syslogSet = true;
  }

  vTaskDelay(pdMS_TO_TICKS(2000));
}

void sntp_sync_callback_fn(timeval *timeVal) {
  time_t     now = (time_t)timeVal->tv_sec;
  char       strftime_buf[32];
  struct tm *currentTime = localtime(&now);

  strftime(strftime_buf, 32, "%c", currentTime);
  APP_ISR_LOGI("SNTP", "SNTP set time to %s", strftime_buf);
}
