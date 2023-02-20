#include <Arduino.h>
#include <FontConfig.h>
#include <logger.h>

#include "System.h"
#include "Task.h"
#include "TaskDisplay.h"
#include "project_configuration.h"

DisplayTask::DisplayTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system, QueueHandle_t &toDisplay, const char *version) : FreeRTOSTask(TASK_DISPLAY, TaskDisplay, priority, 2048, coreId, displayOnScreen), _toDisplay(toDisplay), _displaySaveMode(false), _version(version), _system(system) {
  start();
}

DisplayTask::~DisplayTask() {
}

void DisplayTask::worker() {
  vTaskDelay(pdMS_TO_TICKS(100));
  if (_system.getBoardConfig()->OledReset != 0) {
    pinMode(_system.getBoardConfig()->OledReset, OUTPUT);
    digitalWrite(_system.getBoardConfig()->OledReset, HIGH);
    delay(1);
    digitalWrite(_system.getBoardConfig()->OledReset, LOW);
    delay(10);
    digitalWrite(_system.getBoardConfig()->OledReset, HIGH);
  }
  Wire.begin(_system.getBoardConfig()->OledSda, _system.getBoardConfig()->OledScl);
  _disp = new SSD1306(&Wire, _system.getBoardConfig()->OledAddr);

  Bitmap bitmap(_disp->getWidth(), _disp->getHeight());
  _disp->display(&bitmap);

  _frameTimeout.setTimeout(2 * 1000);
  _displaySaveModeTimer.setTimeout(10 * 1000);
  if (_system.getUserConfig()->display.turn180) {
    turn180();
  }

  std::shared_ptr<StatusFrame> statusFrame = std::shared_ptr<StatusFrame>(new StatusFrame(_system.getTaskManager().getFreeRTOSTasks()));
  setStatusFrame(statusFrame);

  if (!_system.getUserConfig()->display.alwaysOn) {
    activateDisplaySaveMode();
    setDisplaySaveTimeout(_system.getUserConfig()->display.timeout);
  }
  _stateInfo = _system.getUserConfig()->callsign;

  showSpashScreen("LoRa APRS iGate", _version);

  vTaskDelay(pdMS_TO_TICKS(5000));

  for (;;) {
    if (_system.getUserConfig()->display.overwritePin != 0 && !digitalRead(_system.getUserConfig()->display.overwritePin)) {
      activateDisplay();
    }
    TextFrame *frame;
    bool       res = xQueueReceive(_toDisplay, &frame, pdMS_TO_TICKS(500));
    if (res) {
      Bitmap bitmap(_disp);
      frame->draw(bitmap);
      _disp->display(&bitmap);

      vTaskDelay(pdMS_TO_TICKS(2000));
      delete frame;
    } else {
      if (_disp->isDisplayOn()) {
        Bitmap bitmap(_disp);
        _statusFrame->draw(bitmap);
        _disp->display(&bitmap);

        if (_displaySaveMode) {
          if (_displaySaveModeTimer.isActive() && _displaySaveModeTimer.check()) {
            _disp->displayOff();
            _displaySaveModeTimer.reset();

          } else if (!_displaySaveModeTimer.isActive()) {
            _displaySaveModeTimer.start();
          }
        }
      }
    }
  }
}

void DisplayTask::turn180() {
  _disp->flipScreenVertically();
}

void DisplayTask::activateDisplaySaveMode() {
  _displaySaveMode = true;
}

void DisplayTask::setDisplaySaveTimeout(uint32_t timeout) {
  _displaySaveModeTimer.setTimeout(timeout * 1000);
}

void DisplayTask::activateDisplay() {
  _disp->displayOn();
}

void DisplayTask::setStatusFrame(std::shared_ptr<StatusFrame> frame) {
  _statusFrame = frame;
}

void DisplayTask::showSpashScreen(String firmwareTitle, String version) {
  Bitmap bitmap(_disp);
  bitmap.drawString(0, 10, firmwareTitle);
  bitmap.drawString(0, 20, version);
  bitmap.drawString(0, 35, "by Peter Buchegger");
  bitmap.drawString(30, 45, "OE5BPA");
  _disp->display(&bitmap);
}

void DisplayTask::showStatusScreen(String header, String text) {
  Bitmap bitmap(_disp);
  bitmap.drawString(0, 0, header);
  bitmap.drawStringLF(0, 10, text);
  _disp->display(&bitmap);
}

void TextFrame::draw(Bitmap &bitmap) {
  bitmap.drawString(0, 0, _header);
  bitmap.drawStringLF(0, 10, _text);
}

// cppcheck-suppress unusedFunction
void StatusFrame::draw(Bitmap &bitmap) {
  int       y = 0;
  char      timeStr[9];
  time_t    now = time(NULL);
  struct tm timeInfo;

  localtime_r(&now, &timeInfo);
  strftime(timeStr, sizeof(timeStr), "%T", &timeInfo);
  bitmap.drawString(0, y, String("Time: ") + timeStr);
  y += getSystemFont()->heightInPixel;

  for (FreeRTOSTask *task : _tasks) {
    if (!task->getDisplayOnScreen()) {
      continue;
    }
    int x = bitmap.drawString(0, y, (String(task->getName())).substring(0, String(task->getName()).indexOf("Task")) + ": ");
    if (task->getStateInfo() == "") {
      switch (task->getState()) {
      case Error:
        bitmap.drawString(x, y, "Error");
        break;
      case Warning:
        bitmap.drawString(x, y, "Warning");
      default:
        break;
      }
      bitmap.drawString(x, y, "Okay");
    } else {
      bitmap.drawString(x, y, task->getStateInfo());
    }
    y += getSystemFont()->heightInPixel;
  }
}
