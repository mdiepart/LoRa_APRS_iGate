#ifndef TASK_DISPLAY_H_
#define TASK_DISPLAY_H_

#include <Arduino.h>
#include <SSD1306.h>
#include <TaskManager.h>

#include "System.h"
#include "Timer.h"

class DisplayFrame;
class TextFrame;
class StatusFrame;

class DisplayTask : public FreeRTOSTask {
public:
  DisplayTask(UBaseType_t priority, BaseType_t coreId, const bool displayOnScreen, System &system, QueueHandle_t &toDisplay, const char *version);
  virtual ~DisplayTask();

  void worker() override;

  void showSpashScreen(String firmwareTitle, String version);
  void setStatusFrame(std::shared_ptr<StatusFrame> frame);
  void showStatusScreen(String header, String text);

  void turn180();
  void activateDisplaySaveMode();
  void setDisplaySaveTimeout(uint32_t timeout);

  void activateDisplay();

private:
  QueueHandle_t &_toDisplay;
  bool           _displaySaveMode;
  const char    *_version;

  Timer _displaySaveModeTimer;
  Timer _frameTimeout;

  System                      &_system;
  OLEDDisplay                 *_disp;
  std::shared_ptr<StatusFrame> _statusFrame;
};

class DisplayFrame {
public:
  DisplayFrame() {
  }
  virtual ~DisplayFrame() {
  }
  virtual void draw(Bitmap &bitmap) = 0;
};

class TextFrame : public DisplayFrame {
public:
  TextFrame(String header, String text) : _header(header), _text(text) {
  }
  virtual ~TextFrame() {
  }
  void draw(Bitmap &bitmap) override;

private:
  String _header;
  String _text;
};

class StatusFrame : public DisplayFrame {
public:
  explicit StatusFrame(const std::list<FreeRTOSTask *> &tasks) : _tasks(tasks) {
  }
  virtual ~StatusFrame() {
  }
  void draw(Bitmap &bitmap) override;

private:
  std::list<FreeRTOSTask *> _tasks;
};
#endif
