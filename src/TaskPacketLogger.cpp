#include <FS.h>
#include <SPIFFS.h>
#include <TimeLib.h>
#include <WiFiMulti.h>
#include <logger.h>
#include <queue>
#include <string>

#include "Task.h"
#include "TaskPacketLogger.h"
#include "project_configuration.h"

PacketLoggerTask::PacketLoggerTask(String filename) : Task(TASK_PACKET_LOGGER, TaskPacketLogger) {
  this->filename   = filename;
  enabled          = true;
  log_queue        = std::queue<log_line>();
  total_count      = 0;
  curr_tail_length = 0;
}

PacketLoggerTask::~PacketLoggerTask() {
}

bool PacketLoggerTask::setup(System &system) {
  this->nb_lines        = system.getUserConfig()->packetLogger.nb_lines;
  this->nb_files        = system.getUserConfig()->packetLogger.nb_files;
  this->max_tail_length = system.getUserConfig()->packetLogger.tail_length;
  if (this->max_tail_length > this->nb_lines) {
    this->max_tail_length = nb_lines;
  }
  if (!system.getUserConfig()->packetLogger.active || filename == "" || nb_lines == 0) {
    APP_LOGD(getName(), "Disabled packet logger.");
    enabled = false;
    if (system.getUserConfig()->packetLogger.active) {
      _state = Error;
    } else {
      _state = Okay;
    }
    _stateInfo = "Disabled";
    return true;
  }

  APP_LOGD(getName(), "Setting up packetLogger. Filename is %s. Number of lines is %d. Number of history files is %d.", filename.c_str(), nb_lines, nb_files);
  if (!SPIFFS.begin()) {
    APP_LOGE(getName(), "Could not start SPIFFS...");
    _state     = Error;
    _stateInfo = "Could not start SPIFFS";
    enabled    = false;
    return false;
  } else {
    APP_LOGD(getName(), "SPIFFS started.");
  }

  File csv_file;
  if (!SPIFFS.exists("/" + filename)) {
    APP_LOGD(getName(), "CSV file did not exist. Creating it...");
    csv_file = SPIFFS.open("/" + filename, "w", true);
    if (!csv_file) {
      APP_LOGD(getName(), "Could not create the file...");
      _state     = Error;
      _stateInfo = "Could not create log file";
      enabled    = false;
      return false;
    }
    csv_file.println(HEADER);
    csv_file.close();
    _stateInfo = "Running";
    return true;
  } else {
    csv_file = SPIFFS.open("/" + filename, "r");
    if (!csv_file) {
      APP_LOGD(getName(), "Could not open the csv file to read it.");
    }
    if (csv_file.size() < HEADER.length()) {
      APP_LOGD(getName(), "File size is %d, which is less than header length. Recreating file.", csv_file.size());
      counter = 0;
      csv_file.close();
      SPIFFS.remove("/" + filename);
      csv_file = SPIFFS.open("/" + filename, "w", true);
      if (!csv_file) {
        APP_LOGE(getName(), "Could not re-open file after having removed it...");
        _state     = Error;
        _stateInfo = "Could not create log file";
        enabled    = false;
        return false;
      } else {
        APP_LOGD(getName(), "File recreated successfully.");
      }
      size_t written = csv_file.println(HEADER);
      csv_file.flush();
      APP_LOGD(getName(), "Written %d bytes to file.", written);
      _stateInfo = "Running";
      return true;
    } else {
      // File exists, look for last '\n'
      csv_file.seek(-2, SeekEnd); // Place us before the last char of the file which should be a LF
      while (csv_file.peek() != '\n') {
        if (csv_file.position() > 0) {
          csv_file.seek(-1, SeekCur);
        } else {
          APP_LOGD(getName(), "Could not find a valid previous entry in the file. Recreating it.");
          csv_file.close();
          SPIFFS.remove("/" + filename);
          csv_file = SPIFFS.open("/" + filename, "w", true);
          csv_file.println(HEADER);
          csv_file.close();
          _stateInfo = "Running";
          return true;
        }
      }
      csv_file.seek(1, SeekCur);
      // Read the number, store it to counter
      APP_LOGD(getName(), "Reading previous number from position %d.", csv_file.position());
      String prev_number = csv_file.readStringUntil(SEPARATOR[0]);
      APP_LOGD(getName(), "prev_number is \"%s\"", prev_number.c_str());
      long int n = prev_number.toInt();
      counter    = (n < SIZE_MAX) ? n + 1 : SIZE_MAX;
      APP_LOGD(getName(), "Found a valid previous entry in packets logs. Counter initialized to %d.", counter);
      csv_file.close();
    }
  }

  getTail(false);

  _stateInfo = "Running";
  return true;
}

bool PacketLoggerTask::loop(System &system) {
  if (!enabled) {
    return true;
  }

  if (counter >= nb_lines) {
    rotate(system);
    counter = 0;
  }

  File csv_file;
  if (!log_queue.empty()) {
    csv_file = SPIFFS.open("/" + filename, "a");
    if (!csv_file) {
      APP_LOGE(getName(), "Could not open csv file to log packets...");
      return false;
    }
  } else {
    return true;
  }

  while (!log_queue.empty() && csv_file) {
    log_line   line  = log_queue.front();
    const char fmt[] = "%d" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%.1f" SEPARATOR "%.1f" SEPARATOR "%.1f\n";
    csv_file.printf(fmt, counter, line.timestamp, line.callsign, line.target, line.path, line.data, line.RSSI, line.SNR, line.freq_error);
    log_queue.pop();
    counter++;
    total_count++;

    if (curr_tail_length >= max_tail_length) {
      tail = tail.substring(tail.indexOf("\n") + 1);
    } else {
      curr_tail_length++;
    }
    tail += String(counter) + SEPARATOR + line.timestamp + SEPARATOR + line.callsign + SEPARATOR + line.target + SEPARATOR + line.path;
    tail += SEPARATOR + String(line.data) + SEPARATOR + String(line.RSSI, 1) + SEPARATOR + String(line.SNR, 1) + SEPARATOR + String(line.freq_error, 1) + "\n";
  }

  _stateInfo = "Logged " + String(total_count) + " packets since the device started";
  csv_file.close();

  return true;
}

void PacketLoggerTask::logPacket(const String &callsign, const String &target, const String &path, const String &data, float RSSI, float SNR, float frequency_error) {
  // TODO check strings lengths
  if (log_queue.size() >= QUEUE_SIZE) {
    return;
  }

  TimeElements tm;
  breakTime(now(), tm);
  char timestamp[32];
  int  written = snprintf(timestamp, 31, "%04d-%02d-%02dT%02d:%02d:%02dZ", 1970 + tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);

  log_line line = {0};
  strncpy(line.timestamp, timestamp, min<uint>(20, written));
  strncpy(line.callsign, callsign.c_str(), min<uint>(9, callsign.length()));
  strncpy(line.target, target.c_str(), min<uint>(6, target.length()));
  strncpy(line.path, path.c_str(), min<uint>(19, path.length()));
  strncpy(line.data, data.c_str(), min<uint>(253, data.length()));
  line.RSSI       = RSSI;
  line.SNR        = SNR;
  line.freq_error = frequency_error;
  log_queue.push(line);
}

void PacketLoggerTask::rotate(System &system) {
  if (nb_files == 0) {
    SPIFFS.remove("/" + filename);
    return;
  }

  // Remove oldest file if it exists
  char target_file[32] = {0};
  char origin_file[32] = {0};
  snprintf(origin_file, 32, "/%s.%d", filename.c_str(), nb_files - 1);

  if (SPIFFS.remove(origin_file) == ESP_OK) {
    APP_LOGD(getName(), "Deleted file %s.", origin_file);
  } else {
    // File might not exist, in this case, remove will return ESP_ERR_NVS_NOT_FOUND
    APP_LOGD(getName(), "Could not delete file %s.", origin_file);
  }

  for (int i = nb_files - 1; i > 0; i--) {
    snprintf(origin_file, 32, "/%s.%d", filename.c_str(), i - 1);
    snprintf(target_file, 32, "/%s.%d", filename.c_str(), i);
    if (SPIFFS.exists(origin_file)) {
      SPIFFS.rename(origin_file, target_file);
      APP_LOGD(getName(), "Moved file %s to %s.", origin_file, target_file);
    }
  }

  snprintf(origin_file, 32, "/%s", filename.c_str());
  snprintf(target_file, 32, "/%s.0", filename.c_str());

  SPIFFS.rename(origin_file, target_file);

  File csv_file = SPIFFS.open(origin_file, "w", true);
  csv_file.println(HEADER);
  csv_file.close();
}

String PacketLoggerTask::getTail(bool use_cache) {
  if (use_cache == true && !tail.isEmpty()) {
    return tail;
  }

  tail.clear();

  File csv_file = SPIFFS.open("/" + filename, "r");
  if (!csv_file) {
    return "Error opening logs file...";
  }

  unsigned int length = min<uint>(max_tail_length, counter);
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
    tail += csv_file.readStringUntil('\n') + "\n";
    length--;
    curr_tail_length++;
  }

  csv_file.close();

  return tail;
}

bool PacketLoggerTask::getFullLogs(WiFiClient &client) {
  client.println(HEADER);
  size_t counter = 0;
  File   csv_file;
  String log_line;
  char   file[32];

  for (int i = nb_files - 1; i >= 0; i--) {
    snprintf(file, 31, "/%s.%d", filename.c_str(), i);
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
  csv_file = SPIFFS.open("/" + filename, "r");
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
