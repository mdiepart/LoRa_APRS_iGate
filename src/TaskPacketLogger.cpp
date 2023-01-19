#include <APRSMessage.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiMulti.h>
#include <ctime>
#include <logger.h>
#include <queue>
#include <string>

#include "Task.h"
#include "TaskPacketLogger.h"
#include "project_configuration.h"

PacketLoggerTask::PacketLoggerTask(UBaseType_t priority, BaseType_t coreId, System &system, const String filename, QueueHandle_t &toPacketLogger) : FreeRTOSTask(TASK_PACKET_LOGGER, TaskPacketLogger, priority, 3072, coreId), _counter(0), _curr_tail_length(0), _total_count(0), _filename(filename), _tail(""), _system(system), _toPacketLogger(toPacketLogger) {
  _nb_lines        = _system.getUserConfig()->packetLogger.nb_lines;
  _nb_files        = _system.getUserConfig()->packetLogger.nb_files;
  _max_tail_length = std::min<size_t>(system.getUserConfig()->packetLogger.tail_length, _nb_lines);
  start();
}

PacketLoggerTask::~PacketLoggerTask() {
}

void PacketLoggerTask::worker() {
  if (!_system.getUserConfig()->packetLogger.active || _filename == "" || _nb_lines == 0) {
    APP_LOGE(getName(), "Could not enable packet logger.");
    _state     = Error;
    _stateInfo = "Disabled";
    return; // Impossible to execute task. Return now, it will get deleted
  }

  APP_LOGD(getName(), "Setting up packetLogger. Filename is %s. Number of lines is %d. Number of history files is %d.", _filename.c_str(), _nb_lines, _nb_files);
  if (!SPIFFS.begin()) {
    APP_LOGE(getName(), "Could not start SPIFFS...");
    _state     = Error;
    _stateInfo = "SPIFFS error";
    return;
  }

  File csv_file;
  if (!SPIFFS.exists("/" + _filename)) {
    APP_LOGD(getName(), "CSV file did not exist. Creating it...");
    csv_file = SPIFFS.open("/" + _filename, "w", true);
    if (!csv_file) {
      APP_LOGD(getName(), "Could not create the file...");
      _state     = Error;
      _stateInfo = "File error";
      return;
    }
    csv_file.println(HEADER);
    csv_file.close();
  } else {
    csv_file = SPIFFS.open("/" + _filename, "r");
    if (!csv_file) {
      APP_LOGD(getName(), "Could not open the csv file to read it.");
      _state     = Error;
      _stateInfo = "File error";
      return;
    }
    if (csv_file.size() < HEADER.length()) {
      APP_LOGD(getName(), "File size is %d, which is less than header length. Recreating file.", csv_file.size());
      _counter = 0;
      csv_file.close();
      SPIFFS.remove("/" + _filename);
      csv_file = SPIFFS.open("/" + _filename, "w", true);
      if (!csv_file) {
        APP_LOGE(getName(), "Could not re-open file after having removed it...");
        _state     = Error;
        _stateInfo = "File error";
        return;
      }
      csv_file.println(HEADER);
      csv_file.close();
      _stateInfo = "Running";
    } else {
      // File exists, look for last '\n'
      bool parse_number = true;
      csv_file.seek(-2, SeekEnd); // Place us before the last char of the file which should be a LF
      while (csv_file.peek() != '\n') {
        if (csv_file.position() > 0) {
          csv_file.seek(-1, SeekCur);
        } else {
          APP_LOGD(getName(), "Could not find a valid previous entry in the file. Recreating it.");
          csv_file.close();
          SPIFFS.remove("/" + _filename);
          csv_file = SPIFFS.open("/" + _filename, "w", true);
          if (!csv_file) {
            APP_LOGE(getName(), "Could not re-open file after having removed it...");
            _state     = Error;
            _stateInfo = "File error";
            return;
          }
          csv_file.println(HEADER);
          parse_number = false;
          break;
        }
      }
      if (parse_number) {
        csv_file.seek(1, SeekCur);
        // Read the number, store it to counter
        String prev_number = csv_file.readStringUntil(SEPARATOR[0]);
        APP_LOGD(getName(), "prev_number is %s", prev_number.c_str());
        long int n = prev_number.toInt();
        APP_LOGD(getName(), "n is thus equal to %d", n);
        _counter = (n < SIZE_MAX) ? n + 1 : SIZE_MAX;
        APP_LOGD(getName(), "Found a valid previous entry in packets logs. Counter initialized to %d.", _counter);
      }
      csv_file.close();
      getTail(false);
    }
  }
  _stateInfo = "Running";
  logEntry entry;
  for (;;) {
    // Wait untill we have an entry to add to log
    xQueueReceive(_toPacketLogger, &entry, portMAX_DELAY);

    struct tm timeInfo;
    localtime_r(&entry.rxTime, &timeInfo);

    if (_counter >= _nb_lines) {
      rotate();
      _counter = 0;
    }
    csv_file = SPIFFS.open("/" + _filename, "a");
    if (!csv_file) {
      APP_LOGE(getName(), "Could not open csv file to log packets...");
      _stateInfo = "File error";
      _state     = Error;
      return;
    }

    int        lineLength;
    char      *line  = NULL;
    const char fmt[] = "%d" /* counter */ SEPARATOR "%04d-%02d-%02dT%02d:%02d:%02dZ" /* timestamp*/ SEPARATOR                        //
                       "%s" /* callsign */ SEPARATOR "%s" /* target */ SEPARATOR "%s" /* path */ SEPARATOR "%s" /* data */ SEPARATOR //
                       "%.1f" /* RSSI */ SEPARATOR "%.1f" /* SNR */ SEPARATOR "%.1f\n" /* freq_error */;

    /* Create line buffer */
    if (entry.msg == NULL) {
      lineLength = snprintf(nullptr, 0, fmt, _counter, timeInfo.tm_year, timeInfo.tm_mon, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, //
                            " ", " ", " ", "INVALID PACKET", entry.rssi, entry.snr, entry.freq_error);
      if (lineLength > 0) {
        line = new char[lineLength + 1];
        if (line != NULL) {
          snprintf(line, lineLength + 1, fmt, _counter, timeInfo.tm_year, timeInfo.tm_mon, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, //
                   " ", " ", " ", "INVALID PACKET", entry.rssi, entry.snr, entry.freq_error);
        }
      }
    } else {
      lineLength = snprintf(nullptr, 0, fmt, _counter, timeInfo.tm_year, timeInfo.tm_mon, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, //
                            entry.msg->getSource().c_str(), entry.msg->getDestination().c_str(), entry.msg->getPath().c_str(), entry.msg->getRawBody().c_str(), //
                            entry.rssi, entry.snr, entry.freq_error);
      if (lineLength > 0) {
        line = new char[lineLength + 1];
        if (line != NULL) {
          snprintf(line, lineLength + 1, fmt, _counter, timeInfo.tm_year, timeInfo.tm_mon, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, //
                   entry.msg->getSource().c_str(), entry.msg->getDestination().c_str(), entry.msg->getPath().c_str(), entry.msg->getRawBody().c_str(),           //
                   entry.rssi, entry.snr, entry.freq_error);
        }
      }
    }

    /* print the line to file and tail */
    if (line != NULL) {
      /* remove oldest line from _tail if it already is the max length */
      if (_curr_tail_length >= _max_tail_length) {
        _tail = _tail.substring(_tail.indexOf("\n") + 1);
      } else {
        _curr_tail_length++;
      }
      _tail += line;
      csv_file.print(line);
      delete line;
    }

    if (entry.msg != NULL) {
      delete entry.msg;
    }
    csv_file.close();
    _counter++;
    _total_count++;
    _stateInfo = "Logged " + String(_total_count) + " packets since the device started";
  }
}

void PacketLoggerTask::rotate() {
  if (_nb_files == 0) {
    SPIFFS.remove("/" + _filename);
    return;
  }

  // Remove oldest file if it exists
  char target_file[32] = {0};
  char origin_file[32] = {0};
  snprintf(origin_file, 32, "/%s.%d", _filename.c_str(), _nb_files - 1);

  if (SPIFFS.remove(origin_file) == ESP_OK) {
    APP_LOGD(getName(), "Deleted file %s.", origin_file);
  } else {
    // File might not exist, in this case, remove will return ESP_ERR_NVS_NOT_FOUND
    APP_LOGD(getName(), "Could not delete file %s.", origin_file);
  }

  for (int i = _nb_files - 1; i > 0; i--) {
    snprintf(origin_file, 32, "/%s.%d", _filename.c_str(), i - 1);
    snprintf(target_file, 32, "/%s.%d", _filename.c_str(), i);
    if (SPIFFS.exists(origin_file)) {
      SPIFFS.rename(origin_file, target_file);
      APP_LOGD(getName(), "Moved file %s to %s.", origin_file, target_file);
    }
  }

  snprintf(origin_file, 32, "/%s", _filename.c_str());
  snprintf(target_file, 32, "/%s.0", _filename.c_str());

  SPIFFS.rename(origin_file, target_file);

  File csv_file = SPIFFS.open(origin_file, "w", true);
  csv_file.println(HEADER);
  csv_file.close();
}

String PacketLoggerTask::getTail(bool use_cache) {
  if (use_cache == true && !_tail.isEmpty()) {
    return _tail;
  }

  _tail.clear();

  File csv_file = SPIFFS.open("/" + _filename, "r");
  if (!csv_file) {
    return "Error opening logs file...";
  }

  unsigned int length = min<uint>(_max_tail_length, _counter);
  unsigned int i      = length;
  csv_file.seek(-2, SeekEnd); // Rewind file to just before the last LF
  while (csv_file.position() > 0) {
    if (csv_file.peek() == '\n') {
      i--;
    }
    if (i == 0) {
      break;
    }
    csv_file.seek(-1, SeekCur);
  }
  csv_file.seek(1, SeekCur);

  while ((length > 0) && (csv_file.position() < csv_file.size())) {
    _tail += csv_file.readStringUntil('\n') + "\n";
    length--;
    _curr_tail_length++;
  }

  csv_file.close();

  return _tail;
}

bool PacketLoggerTask::getFullLogs(WiFiClient &client) {
  client.println(HEADER);
  size_t counter = 0;
  File   csv_file;
  String log_line;
  char   file[32];

  for (int i = _nb_files - 1; i >= 0; i--) {
    snprintf(file, 31, "/%s.%d", _filename.c_str(), i);
    csv_file = SPIFFS.open(file, "r");
    if (csv_file) {
      csv_file.readStringUntil('\n');
      while (csv_file.position() < csv_file.size()) {
        csv_file.readStringUntil('\t');
        log_line = csv_file.readStringUntil('\n');
        client.printf("%d\t%s\n", counter++, log_line.c_str());
      }
      csv_file.close();
    }
  }

  // Read from current file
  csv_file = SPIFFS.open("/" + _filename, "r");
  if (!csv_file) {
    return false;
  }
  csv_file.readStringUntil('\n'); // Discard header
  while (csv_file.position() < csv_file.size()) {
    csv_file.readStringUntil('\t');
    log_line = csv_file.readStringUntil('\n');
    client.printf("%d\t%s\n", counter++, log_line.c_str());
  }
  csv_file.close();

  return true;
}

logEntry::logEntry() : msg(NULL), rxTime(0), rssi(0), snr(0), freq_error(0) {
}

logEntry::logEntry(APRSMessage *msg, time_t rxTime, float rssi, float snr, float freq_error) : msg(msg), rxTime(rxTime), rssi(rssi), snr(snr), freq_error(freq_error) {
}
