#ifndef PACKET_LOGGER_H_
#define PACKET_LOGGER_H_

#include <APRSMessage.h>
#include <FS.h>
#include <TaskManager.h>
#include <WiFiMulti.h>
#include <esp_https_server.h>
#include <queue>
#include <stdio.h>

#define SEPARATOR "\t"

class PacketLoggerTask : public FreeRTOSTask {

public:
  PacketLoggerTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system, const String filename, QueueHandle_t &queueHandle);
  virtual ~PacketLoggerTask();

  void worker() override;

  String getTail(bool use_cache = true);
  bool   getFullLogs(httpd_req_t *req);

private:
  bool rotate();

  size_t         _nb_lines;
  size_t         _nb_files;
  size_t         _counter;
  size_t         _max_tail_length;
  size_t         _curr_tail_length;
  unsigned int   _total_count;
  String         _filename;
  String         _tail;
  const String   HEADER = String("NUMBER" SEPARATOR "TIMESTAMP" SEPARATOR "CALLSIGN" SEPARATOR "TARGET" SEPARATOR "PATH" SEPARATOR "DATA" SEPARATOR "RSSI" SEPARATOR "SNR" SEPARATOR "FREQ_ERROR\n");
  System        &_system;
  QueueHandle_t &_toPacketLogger;
};

struct logEntry {
  logEntry();
  logEntry(APRSMessage *msg, time_t rxTime, float rssi, float snr, float freq_error);

  APRSMessage *msg;
  time_t       rxTime;
  float        rssi;
  float        snr;
  float        freq_error;
};

#endif
