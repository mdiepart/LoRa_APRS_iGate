#include <Task.h>
#include <TimeLib.h>
#include <logger.h>

#include "TaskPacketLogger.h"
#include "TaskRadiolib.h"

RadiolibTask::RadiolibTask(TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem) : Task(TASK_RADIOLIB, TaskRadiolib), _fromModem(fromModem), _toModem(toModem) {
}

RadiolibTask::~RadiolibTask() {
  radio->clearDio0Action();
}

volatile bool RadiolibTask::enableInterrupt = true;  // Need to catch interrupt or not.
volatile bool RadiolibTask::operationDone   = false; // Caught IRQ or not.

void RadiolibTask::setFlag(void) {
  if (!enableInterrupt) {
    return;
  }

  operationDone = true;
}

bool RadiolibTask::setup(System &system) {
  SPI.begin(system.getBoardConfig()->LoraSck, system.getBoardConfig()->LoraMiso, system.getBoardConfig()->LoraMosi, system.getBoardConfig()->LoraCS);
  module = new Module(system.getBoardConfig()->LoraCS, system.getBoardConfig()->LoraIRQ, system.getBoardConfig()->LoraReset);
  radio  = new SX1278(module);

  config = system.getUserConfig()->lora;

  rxEnable = true;
  txEnable = config.tx_enable;

  float freqMHz = (float)config.frequencyRx / 1000000;
  float BWkHz   = (float)config.signalBandwidth / 1000;

  const uint16_t preambleLength = 8;

  int16_t state = radio->begin(freqMHz, BWkHz, config.spreadingFactor, config.codingRate4, RADIOLIB_SX127X_SYNC_WORD, config.power, preambleLength, config.gainRx);
  if (state != RADIOLIB_ERR_NONE) {
    switch (state) {
    case RADIOLIB_ERR_INVALID_FREQUENCY:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied frequency value (%fMHz) is invalid for this module.", timeString().c_str(), freqMHz);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_BANDWIDTH:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied bandwidth value (%fkHz) is invalid for this module. Should be 7800, 10400, 15600, 20800, 31250, 41700 ,62500, 125000, 250000, 500000.", timeString().c_str(), BWkHz);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied spreading factor value (%d) is invalid for this module.", timeString().c_str(), config.spreadingFactor);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_CODING_RATE:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied coding rate value (%d) is invalid for this module.", timeString().c_str(), config.codingRate4);
      rxEnable = false;
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied output power value (%d) is invalid for this module.", timeString().c_str(), config.power);
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied preamble length is invalid.", timeString().c_str());
      txEnable = false;
      break;
    case RADIOLIB_ERR_INVALID_GAIN:
      logger.error(getName(), "[%s] SX1278 init failed, The supplied gain value (%d) is invalid.", timeString().c_str(), config.gainRx);
      rxEnable = false;
      break;
    default:
      logger.error(getName(), "[%s] SX1278 init failed, code %d", timeString().c_str(), state);
      rxEnable = false;
      txEnable = false;
    }
    _stateInfo = "LoRa-Modem failed";
    _state     = Error;
  }

  state = radio->setCRC(true);
  if (state != RADIOLIB_ERR_NONE) {
    logger.error(getName(), "[%s] setCRC failed, code %d", timeString().c_str(), state);
    _stateInfo = "LoRa-Modem failed";
    _state     = Error;
  }

  radio->setDio0Action(setFlag);

  if (rxEnable) {
    int state = startRX(RADIOLIB_SX127X_RXCONTINUOUS);
    if (state != RADIOLIB_ERR_NONE) {
      logger.error(getName(), "[%s] startRX failed, code %d", timeString().c_str(), state);
      rxEnable   = false;
      _stateInfo = "LoRa-Modem failed";
      _state     = Error;
    }
  }

  preambleDurationMilliSec = ((uint64_t)(preambleLength + 4) << (config.spreadingFactor + 10 /* to milli-sec */)) / config.signalBandwidth;

  _stateInfo = "";
  return true;
}

int  transmissionState = RADIOLIB_ERR_NONE;
bool transmitFlag      = false; // Transmitting or not.

bool RadiolibTask::loop(System &system) {
  if (operationDone) { // occurs interrupt.
    enableInterrupt = false;

    if (transmitFlag) { // transmitted.
      if (transmissionState == RADIOLIB_ERR_NONE) {
        logger.debug(getName(), "[%s] TX done", timeString().c_str());

      } else {
        logger.error(getName(), "[%s] transmitFlag failed, code %d", timeString().c_str(), transmissionState);
      }
      operationDone = false;
      transmitFlag  = false;

      txWaitTimer.setTimeout(preambleDurationMilliSec * 2);
      txWaitTimer.start();

    } else { // received.
      String str;
      int    state = radio->readData(str);

      if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        // Log an error
        logger.error(getName(), "[%s] Received corrupt packet (CRC check failed)", timeString().c_str());
        system.getPacketLogger()->logPacket("", "", "", "CRC error", radio->getRSSI(), radio->getSNR(), radio->getFrequencyError());
      } else if (state != RADIOLIB_ERR_NONE) {
        logger.error(getName(), "[%s] readData failed, code %d", timeString().c_str(), state);
      } else {
        if (str.substring(0, 3) != "<\xff\x01") {
          logger.debug(getName(), "[%s] Unknown packet '%s' with RSSI %.0fdBm, SNR %.2fdB and FreqErr %fHz", timeString().c_str(), str.c_str(), radio->getRSSI(), radio->getSNR(), -radio->getFrequencyError());
        } else {
          std::shared_ptr<APRSMessage> msg = std::shared_ptr<APRSMessage>(new APRSMessage());
          msg->decode(str.substring(3));
          _fromModem.addElement(msg);
          logger.debug(getName(), "[%s] Received packet '%s' with RSSI %.0fdBm, SNR %.2fdB and FreqErr %fHz", timeString().c_str(), msg->toString().c_str(), radio->getRSSI(), radio->getSNR(), -radio->getFrequencyError());
          system.getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("LoRa", msg->toString().c_str())));
          TimeElements tm;
          breakTime(now(), tm);
          char timestamp[32];
          snprintf(timestamp, 31, "%04d-%02d-%02dT%02d:%02d:%02dZ", 1970 + tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
          system.getPacketLogger()->logPacket(msg->getSource(), msg->getDestination(), msg->getPath(), msg->getBody()->getData(), radio->getRSSI(), radio->getSNR(), radio->getFrequencyError());
        }
      }
      operationDone = false;
    }

    if (rxEnable) {
      int state = startRX(RADIOLIB_SX127X_RXCONTINUOUS);
      if (state != RADIOLIB_ERR_NONE) {
        logger.error(getName(), "[%s] startRX failed, code %d", timeString().c_str(), state);
        rxEnable = false;
      }
    }

    enableInterrupt = true;
  } else { // not interrupt.
    if (!txWaitTimer.check()) {
    } else {
      if (!txEnable) {
        // logger.debug(getName(), "[%s] TX is not enabled", timeString().c_str());
      } else {
        if (transmitFlag) {
          // logger.debug(getName(), "[%s] TX signal detected. Waiting TX", timeString().c_str());
        } else {
          if (!_toModem.empty()) {
            if (config.frequencyRx == config.frequencyTx && (radio->getModemStatus() & 0x01) == 0x01) {
              // logger.debug(getName(), "[%s] RX signal detected. Waiting TX", timeString().c_str());
            } else {
              std::shared_ptr<APRSMessage> msg = _toModem.getElement();
              logger.debug(getName(), "[%s] Transmitting packet '%s'", timeString().c_str(), msg->toString().c_str());

              int16_t state = startTX("<\xff\x01" + msg->encode());
              if (state != RADIOLIB_ERR_NONE) {
                logger.error(getName(), "[%s] startTX failed, code %d", timeString().c_str(), state);
                txEnable = false;
                return true;
              }
            }
          }
        }
      }
    }
  }

  return true;
}

int16_t RadiolibTask::startRX(uint8_t mode) {
  if (config.frequencyTx != config.frequencyRx) {
    int16_t state = radio->setFrequency((float)config.frequencyRx / 1000000);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  }

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
  transmitFlag      = true;
  return RADIOLIB_ERR_NONE;
}
