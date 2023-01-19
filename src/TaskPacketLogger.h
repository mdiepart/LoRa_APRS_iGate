#ifndef PACKET_LOGGER_H_
#define PACKET_LOGGER_H_

#include <FS.h>
#include <TaskManager.h>
#include <WiFiMulti.h>
#include <queue>
#include <stdio.h>

#define SEPARATOR "\t"

class PacketLoggerTask : public FreeRTOSTask {

public:
  PacketLoggerTask(UBaseType_t priority, BaseType_t coreId, System &system, const String filename);
  virtual ~PacketLoggerTask();

  void worker() override;

  void   logPacket(const String &callsign, const String &target, const String &path, const String &data, float RSSI, float SNR, float frequency_error);
  String getTail(bool use_cache = true);
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

  void rotate();

  size_t               _nb_lines;
  size_t               _nb_files;
  size_t               _counter;
  size_t               _max_tail_length;
  size_t               _curr_tail_length;
  unsigned int         _total_count;
  String               _filename;
  String               _tail;
  const String         HEADER     = String("NUMBER" SEPARATOR "TIMESTAMP" SEPARATOR "CALLSIGN" SEPARATOR "TARGET" SEPARATOR "PATH" SEPARATOR "DATA" SEPARATOR "RSSI" SEPARATOR "SNR" SEPARATOR "FREQ_ERROR");
  const size_t         QUEUE_SIZE = 5;
  std::queue<log_line> _log_queue;
  System              &_system;
};

#endif
