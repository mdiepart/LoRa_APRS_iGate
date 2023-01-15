#include "configuration.h"
#include <SPIFFS.h>
#include <logger.h>

#define MODULE_NAME "ConfigurationManagement"

ConfigurationManagement::ConfigurationManagement(String FilePath) : mFilePath(FilePath) {
  if (!SPIFFS.begin(true)) {
    logger.log(logging::LoggerLevel::LOG_INFO, MODULE_NAME, "Mounting SPIFFS was not possible. Trying to format SPIFFS...");
    SPIFFS.format();
    if (!SPIFFS.begin()) {
      logger.log(logging::LoggerLevel::LOG_ERROR, MODULE_NAME, "Formatting SPIFFS was not okay!");
    }
  }
}

ConfigurationManagement::~ConfigurationManagement() {
}

void ConfigurationManagement::readConfiguration(Configuration &conf) {
  File file = SPIFFS.open(mFilePath);
  if (!file) {
    APP_LOGE(MODULE_NAME, "Failed to open file for reading, using default configuration.");
    return;
  }
  DynamicJsonDocument  data(2048);
  DeserializationError error = deserializeJson(data, file);
  if (error) {
    APP_LOGW(MODULE_NAME, "Failed to read file, using default configuration.");
  }
  // serializeJson(data, Serial);
  // Serial.println();
  file.close();

  readProjectConfiguration(data, conf);

  // update config in memory to get the new fields:
  // writeConfiguration(logger, conf);
}

void ConfigurationManagement::writeConfiguration(Configuration &conf) {
  File file = SPIFFS.open(mFilePath, "w");
  if (!file) {
    APP_LOGE(MODULE_NAME, "Failed to open file for writing...");
    return;
  }
  DynamicJsonDocument data(2048);

  writeProjectConfiguration(conf, data);

  serializeJson(data, file);
  // serializeJson(data, Serial);
  // Serial.println();
  file.close();
}
