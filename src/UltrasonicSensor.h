#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <Arduino.h>

class UltrasonicSensor {
public:
  UltrasonicSensor(HardwareSerial& serialPort, bool mockMode = false);

  void begin(long baudRate = 9600);
  void update();
  uint16_t getDistance() const;
  bool isMockMode() const;

private:
  HardwareSerial& _serialPort;
  bool _mockMode;
  uint16_t _currentDistance;
};

#endif // ULTRASONIC_SENSOR_H
