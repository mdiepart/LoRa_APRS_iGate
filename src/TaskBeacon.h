#ifndef TASK_BEACON_H_
#define TASK_BEACON_H_

#include <OneButton.h>
#include <TinyGPS++.h>

#include <APRSMessage.h>
#include <TaskMQTT.h>
#include <TaskManager.h>

class BeaconTask : public FreeRTOSTask {
public:
  BeaconTask(UBaseType_t priority, BaseType_t coreId, System &system, QueueHandle_t &toModem, QueueHandle_t &toAprsIs);

  void worker() override;

private:
  QueueHandle_t &_toModem;
  QueueHandle_t &_toAprsIs;

  APRSMessage _beaconMsg;
  Timer       _beacon_timer;

  System        *_system;
  HardwareSerial _ss;
  TinyGPSPlus    _gps;
  bool           _useGps;
  bool           _beaconMsgReady;
  bool           _aprsBeaconSent;

  void onGpsSSReceive();
  bool buildBeaconMsg();

  static uint      _instances;
  static OneButton _userButton;
  static bool      _send_update;
  static void      pushButton();
};

#endif
