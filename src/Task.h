#ifndef TASK_H_
#define TASK_H_

enum TaskNames {
  TaskDisplay = 0,
  TaskAprsIs,
  TaskEth,
  TaskFtp,
  TaskModem,
  TaskRadiolib,
  TaskNtp,
  TaskOta,
  TaskWifi,
  TaskRouter,
  TaskMQTT,
  TaskBeacon,
  TaskWeb,
  TaskWebClient,
  TaskPacketLogger,
  TaskSize,
};

#define TASK_APRS_IS       "AprsIsTask"
#define TASK_ETH           "EthTask"
#define TASK_FTP           "FTPTask"
#define TASK_MODEM         "ModemTask"
#define TASK_RADIOLIB      "RadiolibTask"
#define TASK_NTP           "NTPTask"
#define TASK_OTA           "OTATask"
#define TASK_WIFI          "WifiTask"
#define TASK_ROUTER        "RouterTask"
#define TASK_MQTT          "MQTTTask"
#define TASK_BEACON        "BeaconTask"
#define TASK_WEB           "WebTask"
#define TASK_PACKET_LOGGER "PacketLoggerTask"
#define TASK_DISPLAY       "DisplayTask"

#endif
