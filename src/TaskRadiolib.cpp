#include <Task.h>
#include <TimeLib.h>
#include <logger.h>

#include "TaskPacketLogger.h"
#include "TaskRadiolib.h"

int transmissionState = RADIOLIB_ERR_NONE;

static xTaskHandle       taskToNotify      = NULL; // Used for direct-to-task notification when waiting for tx to complete
static SemaphoreHandle_t radioIRQSemaphore = NULL; // Used by the task when waiting for an RX event to occur
static volatile bool     enableInterrupt   = true; // Need to catch interrupt or not.

static void radioCallback() {
  BaseType_t higherPriorityAwoken = pdFALSE;
  if (taskToNotify == NULL) {
    xSemaphoreGiveFromISR(radioIRQSemaphore, &higherPriorityAwoken);
    portYIELD_FROM_ISR(higherPriorityAwoken);
  } else {
    vTaskNotifyGiveFromISR(taskToNotify, &higherPriorityAwoken);
    portYIELD_FROM_ISR(higherPriorityAwoken);
  }
}

RadiolibTask::RadiolibTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &fromModem, QueueHandle_t &toModem) : FreeRTOSTask(TASK_RADIOLIB, TaskRadiolib, priority, 2560, coreId), _system(system), rxEnable(true), _fromModem(fromModem), _toModem(toModem) {
  config   = _system.getUserConfig()->lora;
  txEnable = config.tx_enable;
  start();
}

RadiolibTask::~RadiolibTask() {
  radio->clearDio0Action();
}

void RadiolibTask::worker() {
  // taskToNotify = handle;
  radioIRQSemaphore = xSemaphoreCreateBinary();
  SPI.begin(_system.getBoardConfig()->LoraSck, _system.getBoardConfig()->LoraMiso, _system.getBoardConfig()->LoraMosi, _system.getBoardConfig()->LoraCS);
  module = new Module(_system.getBoardConfig()->LoraCS, _system.getBoardConfig()->LoraIRQ, _system.getBoardConfig()->LoraReset);
  radio  = new SX1278(module);

  float freqMHz = (float)config.frequencyRx / 1000000;
  float BWkHz   = (float)config.signalBandwidth / 1000;

  const uint16_t preambleLength = 8;

  int16_t state = radio->begin(freqMHz, BWkHz, config.spreadingFactor, config.codingRate4, RADIOLIB_SX127X_SYNC_WORD, config.power, preambleLength, config.gainRx);
  if (state != RADIOLIB_ERR_NONE) {
    switch (state) {
    case RADIOLIB_ERR_INVALID_FREQUENCY:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied frequency value (%fMHz) is invalid for this module.", timeString().c_str(), freqMHz);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_BANDWIDTH:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied bandwidth value (%fkHz) is invalid for this module. Should be 7800, 10400, 15600, 20800, 31250, 41700 ,62500, 125000, 250000, 500000.", timeString().c_str(), BWkHz);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied spreading factor value (%d) is invalid for this module.", timeString().c_str(), config.spreadingFactor);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_CODING_RATE:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied coding rate value (%d) is invalid for this module.", timeString().c_str(), config.codingRate4);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied output power value (%d) is invalid for this module.", timeString().c_str(), config.power);
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied preamble length is invalid.", timeString().c_str());
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_GAIN:
      APP_LOGE(getName(), "[%s] SX1278 init failed, The supplied gain value (%d) is invalid.", timeString().c_str(), config.gainRx);
      rxEnable = false;
      break;
    default:
      APP_LOGE(getName(), "[%s] SX1278 init failed, code %d", timeString().c_str(), state);
      rxEnable = false;
      txEnable = false;
    }
    _stateInfo = "LoRa-Modem failed";
    _state     = Error;
  }

  state = radio->setCRC(true);
  if (state != RADIOLIB_ERR_NONE) {
    APP_LOGE(getName(), "[%s] setCRC failed, code %d", timeString().c_str(), state);
    _stateInfo = "LoRa-Modem failed";
    _state     = Error;
  }

  portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&mutex);
  xQueueReset(_toModem);
  UBaseType_t queueSetSize = 1; /* Queue size + 1 for the binary semaphore */
  if (_system.getUserConfig()->lora.tx_enable) {
    queueSetSize += uxQueueSpacesAvailable(_toModem);
  }

  QueueSetHandle_t modemQueueSet = xQueueCreateSet(queueSetSize);
  bool             queueResult   = pdTRUE;

  if (_system.getUserConfig()->lora.tx_enable) {
    queueResult &= xQueueAddToSet(_toModem, modemQueueSet);
  }

  queueResult &= xQueueAddToSet(radioIRQSemaphore, modemQueueSet);
  taskEXIT_CRITICAL(&mutex);

  if (queueResult == pdFAIL) {
    _state     = Error;
    _stateInfo = "Error with queue set";
    return;
  }

  radio->setDio0Action(radioCallback);

  if (rxEnable) {
    int state = startRX(RADIOLIB_SX127X_RXCONTINUOUS);
    if (state != RADIOLIB_ERR_NONE) {
      APP_LOGE(getName(), "[%s] startRX failed, code %d", timeString().c_str(), state);
      rxEnable   = false;
      _stateInfo = "LoRa-Modem failed";
      _state     = Error;
    }
  }

  if (config.power > 17 && config.tx_enable) {
    radio->setCurrentLimit(140);
  }

  preambleDurationMilliSec = ((uint64_t)(preambleLength + 4) << (config.spreadingFactor + 10 /* to milli-sec */)) / config.signalBandwidth;

  _stateInfo = "";

  QueueSetMemberHandle_t activatedMember;

  for (;;) {

    activatedMember = xQueueSelectFromSet(modemQueueSet, portMAX_DELAY);

    if (activatedMember == radioIRQSemaphore) {
      xSemaphoreTake(radioIRQSemaphore, 0);
      String str;
      int    state = radio->readData(str);

      if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        // Log an error
        APP_LOGE(getName(), "[%s] Received corrupt packet (CRC check failed)", timeString().c_str());
        _system.getPacketLogger()->logPacket("", "", "", "CRC error", radio->getRSSI(), radio->getSNR(), radio->getFrequencyError());
      } else if (state != RADIOLIB_ERR_NONE) {
        APP_LOGE(getName(), "[%s] readData failed, code %d", timeString().c_str(), state);
      } else {
        if (str.substring(0, 3) != "<\xff\x01") {
          APP_LOGD(getName(), "[%s] Unknown packet '%s' with RSSI %.0fdBm, SNR %.2fdB and FreqErr %fHz", timeString().c_str(), str.c_str(), radio->getRSSI(), radio->getSNR(), -radio->getFrequencyError());
        } else {
          APRSMessage *msg = new APRSMessage();
          msg->decode(str.substring(3));
          APP_LOGD(getName(), "[%s] Received packet '%s' with RSSI %.0fdBm, SNR %.2fdB and FreqErr %fHz", timeString().c_str(), msg->toString().c_str(), radio->getRSSI(), radio->getSNR(), -radio->getFrequencyError());
          _system.getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("LoRa", msg->toString().c_str())));
          TimeElements tm;
          breakTime(now(), tm);
          char timestamp[32];
          snprintf(timestamp, 31, "%04d-%02d-%02dT%02d:%02d:%02dZ", 1970 + tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
          _system.getPacketLogger()->logPacket(msg->getSource(), msg->getDestination(), msg->getPath(), msg->getBody()->getData(), radio->getRSSI(), radio->getSNR(), radio->getFrequencyError());
          xQueueSend(_fromModem, &msg, pdMS_TO_TICKS(100));
        }
      }
      if (rxEnable) {
        int state = startRX(RADIOLIB_SX127X_RXCONTINUOUS);
        if (state != RADIOLIB_ERR_NONE) {
          APP_LOGE(getName(), "[%s] startRX failed, code %d", timeString().c_str(), state);
          rxEnable = false;
        }
      }

    } else if (activatedMember == _toModem) {
      /* _toModem will not be in the set if tx is not enabled*/
      APRSMessage *msg;
      xQueueReceive(_toModem, &msg, 0);

      APP_LOGD(getName(), "[%s] Transmitting packet '%s'", timeString().c_str(), msg->toString().c_str());

      // Transmit packet
      int16_t state = startTX("<\xff\x01" + msg->encode());
      if (state != RADIOLIB_ERR_NONE) {
        APP_LOGE(getName(), "[%s] startTX failed, code %d", timeString().c_str(), state);
        txEnable = false;
      } else {

        /* Wait 10s max for tx to complete */
        uint32_t txDone = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));
        if (txDone != 1) {
          APP_LOGE(getName(), "TX error: Tx not finished after waiting 10s.");
        } else if (transmissionState != RADIOLIB_ERR_NONE) {
          APP_LOGE(getName(), "[%s] transmitFlag failed, code %d", timeString().c_str(), transmissionState);
        } else {
          APP_LOGD(getName(), "[%s] TX done", timeString().c_str());
        }
      }

      if (rxEnable) {
        int state = startRX(RADIOLIB_SX127X_RXCONTINUOUS);
        if (state != RADIOLIB_ERR_NONE) {
          APP_LOGE(getName(), "[%s] startRX failed, code %d", timeString().c_str(), state);
          rxEnable = false;
        }
      }

      delete msg;

    } else {
      // No member woken up
    }
  }
}

int16_t RadiolibTask::startRX(uint8_t mode) {
  if (config.frequencyTx != config.frequencyRx) {
    int16_t state = radio->setFrequency((float)config.frequencyRx / 1000000);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  }
  taskToNotify = NULL;
  return radio->startReceive(0, mode);
}

int16_t RadiolibTask::startTX(String &str) {
  if (config.frequencyTx != config.frequencyRx) {
    int16_t state = radio->setFrequency((float)config.frequencyTx / 1000000);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  }

  transmissionState = radio->startTransmit(str);
  taskToNotify      = handle;
  return RADIOLIB_ERR_NONE;
}
