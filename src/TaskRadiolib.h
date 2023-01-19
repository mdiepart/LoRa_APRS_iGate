#ifndef TASK_LORA_H_
#define TASK_LORA_H_

#include "project_configuration.h"
#include <APRS-Decoder.h>
#include <RadioLib.h>
#include <TaskManager.h>

class RadiolibTask : public FreeRTOSTask {
public:
  explicit RadiolibTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &fromModem, QueueHandle_t &toModem);
  virtual ~RadiolibTask();

  void worker() override;

private:
  Module *module;
  SX1278 *radio;
  System &_system;

  Configuration::LoRa config;

  bool rxEnable, txEnable;

  QueueHandle_t &_fromModem;
  QueueHandle_t &_toModem;

  int16_t startRX(uint8_t mode);
  int16_t startTX(String &str);

  uint32_t preambleDurationMilliSec;
  Timer    txWaitTimer;
};

#endif
