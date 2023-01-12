#ifndef TASK_BEACON_H_
#define TASK_BEACON_H_

#include <OneButton.h>
#include <TinyGPS++.h>

#include <APRSMessage.h>
#include <TaskMQTT.h>
#include <TaskManager.h>

class BeaconTask : public FreeRTOSTask {
public:
  BeaconTask(UBaseType_t priority, BaseType_t coreId, System &system, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs);

  void worker() override;
  bool sendBeacon();

private:
  TaskQueue<std::shared_ptr<APRSMessage>> *_toModem;
  TaskQueue<std::shared_ptr<APRSMessage>> *_toAprsIs;

  std::shared_ptr<APRSMessage> _beaconMsg;
  Timer                        _beacon_timer;

  System        *_system;
  HardwareSerial _ss;
  TinyGPSPlus    _gps;
  bool           _useGps;

  static uint      _instances;
  static OneButton _userButton;
  static bool      _send_update;
  static void      pushButton();
};

#endif
