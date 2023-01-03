#include <map>

#include <APRS-IS.h>
#include <BoardFinder.h>
#include <System.h>
#include <TaskManager.h>
#include <esp_task_wdt.h>
#include <logger.h>
#include <power_management.h>

#include "TaskAprsIs.h"
#include "TaskBeacon.h"
#include "TaskDisplay.h"
#include "TaskEth.h"
#include "TaskFTP.h"
#include "TaskMQTT.h"
#include "TaskNTP.h"
#include "TaskOTA.h"
#include "TaskPacketLogger.h"
#include "TaskRadiolib.h"
#include "TaskRouter.h"
#include "TaskWeb.h"
#include "TaskWifi.h"
#include "project_configuration.h"

#define VERSION     "22.46.0"
#define MODULE_NAME "Main"

String create_lat_aprs(double lat);
String create_long_aprs(double lng);

TaskQueue<std::shared_ptr<APRSMessage>> toAprsIs;
TaskQueue<std::shared_ptr<APRSMessage>> fromModem;
TaskQueue<std::shared_ptr<APRSMessage>> toModem;
TaskQueue<std::shared_ptr<APRSMessage>> toMQTT;

System        LoRaSystem;
Configuration userConfig;

DisplayTask displayTask;
// ModemTask   modemTask(fromModem, toModem);
RadiolibTask     modemTask(fromModem, toModem);
EthTask          ethTask;
WifiTask         wifiTask;
OTATask          otaTask;
NTPTask          ntpTask;
FTPTask          ftpTask;
MQTTTask         mqttTask(toMQTT);
WebTask          webTask;
AprsIsTask       aprsIsTask(toAprsIs);
RouterTask       routerTask(fromModem, toModem, toAprsIs, toMQTT);
BeaconTask       beaconTask(toModem, toAprsIs);
PacketLoggerTask packetLoggerTask("packets.log");

void setup() {
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);
  Serial.begin(115200);
  LoRaSystem.getLogger().setSerial(&Serial);
  setWiFiLogger(&LoRaSystem.getLogger());
  delay(500);
  LoRaSystem.log_info(MODULE_NAME, "LoRa APRS iGate by OE5BPA (Peter Buchegger)");
  LoRaSystem.log_info(MODULE_NAME, "Version: %s", VERSION);

  switch (esp_reset_reason()) {
  case ESP_RST_TASK_WDT:
    LoRaSystem.log_debug(MODULE_NAME, "Module reset because of watchdog timer.");
    break;
  case ESP_RST_SW:
    LoRaSystem.log_debug(MODULE_NAME, "Module reset following software call.");
    break;
  default:
    LoRaSystem.log_debug(MODULE_NAME, "Module reset for reason %d.", (int)esp_reset_reason());
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

  ProjectConfigurationManagement confmg(LoRaSystem.getLogger());
  confmg.readConfiguration(LoRaSystem.getLogger(), userConfig);

  BoardFinder        finder(boardConfigs);
  BoardConfig const *boardConfig = finder.getBoardConfig(userConfig.board);
  if (!boardConfig) {
    boardConfig = finder.searchBoardConfig(LoRaSystem.getLogger());
    if (!boardConfig) {
      LoRaSystem.log_error(MODULE_NAME, "Board config not set and search failed!");
      while (true)
        ;
    } else {
      userConfig.board = boardConfig->Name;
      confmg.writeConfiguration(LoRaSystem.getLogger(), userConfig);
      LoRaSystem.log_info(MODULE_NAME, "will restart board now!");
      ESP.restart();
    }
  }

  LoRaSystem.log_info(MODULE_NAME, "Board %s loaded.", boardConfig->Name.c_str());

  if (boardConfig->Type == eTTGO_T_Beam_V1_0) {
    Wire.begin(boardConfig->OledSda, boardConfig->OledScl);
    PowerManagement powerManagement;
    if (!powerManagement.begin(Wire)) {
      LoRaSystem.log_info(MODULE_NAME, "AXP192 init done!");
    } else {
      LoRaSystem.log_error(MODULE_NAME, "AXP192 init failed!");
    }
    powerManagement.activateLoRa();
    powerManagement.activateOLED();
    if (userConfig.beacon.use_gps) {
      powerManagement.activateGPS();
    } else {
      powerManagement.deactivateGPS();
    }
  }

  LoRaSystem.setBoardConfig(boardConfig);
  LoRaSystem.setUserConfig(&userConfig);
  LoRaSystem.getTaskManager().addTask(&displayTask);
  LoRaSystem.getTaskManager().addTask(&modemTask);
  LoRaSystem.getTaskManager().addTask(&routerTask);
  LoRaSystem.getTaskManager().addTask(&beaconTask);

  bool tcpip = false;

  if (userConfig.wifi.active) {
    LoRaSystem.getTaskManager().addAlwaysRunTask(&wifiTask);
    tcpip = true;
  }
  if (boardConfig->Type == eETH_BOARD) {
    LoRaSystem.getTaskManager().addAlwaysRunTask(&ethTask);
    tcpip = true;
  }

  if (tcpip) {
    LoRaSystem.getTaskManager().addTask(&otaTask);
    LoRaSystem.getTaskManager().addTask(&ntpTask);
    if (userConfig.ftp.active) {
      LoRaSystem.getTaskManager().addTask(&ftpTask);
    }

    if (userConfig.aprs_is.active) {
      LoRaSystem.getTaskManager().addTask(&aprsIsTask);
    }

    if (userConfig.mqtt.active) {
      LoRaSystem.getTaskManager().addTask(&mqttTask);
    }

    if (userConfig.web.active) {
      LoRaSystem.getTaskManager().addAlwaysRunTask(&webTask);
    }
  }

  LoRaSystem.getTaskManager().addAlwaysRunTask(&packetLoggerTask);
  LoRaSystem.setPacketLogger(&packetLoggerTask);

  esp_task_wdt_reset();
  LoRaSystem.getTaskManager().setup(LoRaSystem);

  LoRaSystem.getDisplay().showSpashScreen("LoRa APRS iGate", VERSION);

  if (userConfig.callsign == "NOCALL-10") {
    LoRaSystem.log_error(MODULE_NAME, "You have to change your settings in 'data/is-cfg.json' and upload it via 'Upload File System image'!");
    LoRaSystem.getDisplay().showStatusScreen("ERROR", "You have to change your settings in 'data/is-cfg.json' and upload it via \"Upload File System image\"!");
    while (true)
      ;
  }
  if ((!userConfig.aprs_is.active) && !(userConfig.digi.active)) {
    LoRaSystem.log_error(MODULE_NAME, "No mode selected (iGate or Digi)! You have to activate one of iGate or Digi.");
    LoRaSystem.getDisplay().showStatusScreen("ERROR", "No mode selected (iGate or Digi)! You have to activate one of iGate or Digi.");
    while (true)
      ;
  }

  if (userConfig.display.overwritePin != 0) {
    pinMode(userConfig.display.overwritePin, INPUT);
    pinMode(userConfig.display.overwritePin, INPUT_PULLUP);
  }

  delay(5000);
  LoRaSystem.log_info(MODULE_NAME, "setup done...");
}

volatile bool syslogSet = false;

void loop() {
  esp_task_wdt_reset();
  LoRaSystem.getTaskManager().loop(LoRaSystem);
  if (LoRaSystem.isWifiOrEthConnected() && LoRaSystem.getUserConfig()->syslog.active && !syslogSet) {
    LoRaSystem.getLogger().setSyslogServer(LoRaSystem.getUserConfig()->syslog.server, LoRaSystem.getUserConfig()->syslog.port, LoRaSystem.getUserConfig()->callsign);
    LoRaSystem.log_info(MODULE_NAME, "System connected after a restart to the network, syslog server set");
    syslogSet = true;
  }
}
