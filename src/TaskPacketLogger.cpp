#include <FS.h>
#include <SPIFFS.h>
#include <TimeLib.h>
#include <queue>
#include <string>

#include "Task.h"
#include "TaskPacketLogger.h"
#include "project_configuration.h"

PacketLoggerTask::PacketLoggerTask(String filename) : Task(TASK_PACKET_LOGGER, TaskPacketLogger) {
  this->filename = filename;
  enabled        = true;
  log_queue      = std::queue<log_line>();
  total_count    = 0;
}

PacketLoggerTask::~PacketLoggerTask() {
}

bool PacketLoggerTask::setup(System &system) {
  this->nb_lines = system.getUserConfig()->packetLogger.nb_lines;
  this->nb_files = system.getUserConfig()->packetLogger.nb_files;
  if (!system.getUserConfig()->packetLogger.active || filename == "" || nb_lines == 0) {
    system.log_debug(getName(), "Disabled packet logger.");
    enabled = false;
    if (system.getUserConfig()->packetLogger.active) {
      _state = Error;
    } else {
      _state = Okay;
    }
    _stateInfo = "Disabled";
    return true;
  }

  system.log_debug(getName(), "Setting up packetLogger. Filename is %s. Number of lines is %d. Number of history files is %d.", filename.c_str(), nb_lines, nb_files);
  if (!SPIFFS.begin()) {
    system.log_error(getName(), "Could not start SPIFFS...");
    _state     = Error;
    _stateInfo = "Could not start SPIFFS";
    enabled    = false;
    return false;
  } else {
    system.log_debug(getName(), "SPIFFS started.");
  }

  File csv_file;
  if (!SPIFFS.exists("/" + filename)) {
    system.log_debug(getName(), "CSV file did not exist. Creating it...");
    csv_file = SPIFFS.open("/" + filename, "w", true);
    if (!csv_file) {
      system.log_debug(getName(), "Could not create the file...");
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
      system.log_debug(getName(), "Could not open the csv file to read it.");
    }
    if (csv_file.size() < HEADER.length()) {
      system.log_debug(getName(), "File size is %d, which is less than header length. Recreating file.", csv_file.size());
      counter = 0;
      csv_file.close();
      SPIFFS.remove("/" + filename);
      csv_file = SPIFFS.open("/" + filename, "w", true);
      if (!csv_file) {
        system.log_error(getName(), "Could not re-open file after having removed it...");
        _state     = Error;
        _stateInfo = "Could not create log file";
        enabled    = false;
        return false;
      } else {
        system.log_debug(getName(), "File recreated successfully.");
      }
      size_t written = csv_file.println(HEADER);
      csv_file.flush();
      system.log_debug(getName(), "Written %d bytes to file.", written);
      _stateInfo = "Running";
      return true;
    } else {
      // File exists, look for last '\n'
      csv_file.seek(-2, SeekEnd); // Place us before the last char of the file which should be a LF
      while (csv_file.peek() != '\n') {
        if (csv_file.position() > 0) {
          csv_file.seek(-1, SeekCur);
        } else {
          system.log_debug(getName(), "Could not find a valid previous entry in the file. Recreating it.");
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
      system.log_debug(getName(), "Reading previous number from position %d.", csv_file.position());
      String prev_number = csv_file.readStringUntil(SEPARATOR[0]);
      system.log_debug(getName(), "prev_number is \"%s\"", prev_number.c_str());
      long int n = prev_number.toInt();
      counter    = (n < SIZE_MAX) ? n + 1 : SIZE_MAX;
      system.log_debug(getName(), "Found a valid previous entry in packets logs. Counter initialized to %d.", counter);
      csv_file.close();
    }
  }
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
      system.log_error(getName(), "Could not open csv file to log packets...");
      return false;
    }
  } else {
    return true;
  }

  while (!log_queue.empty() && csv_file) {
    log_line line = log_queue.front();
    csv_file.printf("%d" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%s" SEPARATOR "%.1f" SEPARATOR "%.1f" SEPARATOR "%.1f\n", counter, line.timestamp, line.callsign, line.target, line.path, line.data, line.RSSI, line.SNR, line.freq_error);
    log_queue.pop();
    counter++;
    total_count++;
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
  strncpy(line.callsign, callsign.c_str(), min<uint>(6, callsign.length()));
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
    system.log_debug(getName(), "Deleted file %s.", origin_file);
  } else {
    // File might not exist, in this case, remove will return ESP_ERR_NVS_NOT_FOUND
    system.log_debug(getName(), "Could not delete file %s.", origin_file);
  }

  for (int i = nb_files - 1; i > 0; i--) {
    snprintf(origin_file, 32, "/%s.%d", filename.c_str(), i - 1);
    snprintf(target_file, 32, "/%s.%d", filename.c_str(), i);
    if (SPIFFS.exists(origin_file)) {
      SPIFFS.rename(origin_file, target_file);
      system.log_debug(getName(), "Moved file %s to %s.", origin_file, target_file);
    }
  }

  snprintf(origin_file, 32, "/%s", filename.c_str());
  snprintf(target_file, 32, "/%s.0", filename.c_str());

  SPIFFS.rename(origin_file, target_file);

  File csv_file = SPIFFS.open(origin_file, "w", true);
  csv_file.println(HEADER);
  csv_file.close();
}
