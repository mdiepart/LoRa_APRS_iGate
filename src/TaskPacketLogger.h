#ifndef PACKET_LOGGER_H_
#define PACKET_LOGGER_H_

#include <FS.h>
#include <TaskManager.h>
#include <WiFiMulti.h>
#include <queue>
#include <stdio.h>

#define SEPARATOR "\t"

class PacketLoggerTask : public Task {

public:
  PacketLoggerTask(String filename);
  virtual ~PacketLoggerTask();

  bool   setup(System &system) override;
  bool   loop(System &system) override;
  void   logPacket(const String &callsign, const String &target, const String &path, const String &data, float RSSI, float SNR, float frequency_error);
  String getTail(unsigned int length);
  bool   getFullLogs(WiFiClient &client);

private:
  typedef struct {
    char  timestamp[21];
    float RSSI;
    float SNR;
    float freq_error;
    char  path[20];
    char  callsign[10];
    char  target[7];
    char  data[253];
  } log_line;

  void   rotate(System &system);
  String filename;

  bool         enabled;
  size_t       nb_lines;
  size_t       nb_files;
  size_t       counter;
  unsigned int total_count;

  // const char          *SEPARATOR  = ";";
  const String         HEADER     = String("NUMBER" SEPARATOR "TIMESTAMP" SEPARATOR "CALLSIGN" SEPARATOR "TARGET" SEPARATOR "PATH" SEPARATOR "DATA" SEPARATOR "RSSI" SEPARATOR "SNR" SEPARATOR "FREQ_ERROR");
  const size_t         QUEUE_SIZE = 5;
  std::queue<log_line> log_queue;
};

#endif
