#include "UltrasonicSensor.h"

UltrasonicSensor::UltrasonicSensor(HardwareSerial& serialPort, bool mockMode)
  : _serialPort(serialPort), _mockMode(mockMode), _currentDistance(0) {}

void UltrasonicSensor::begin(long baudRate) {
  if (_mockMode) {
    Serial.println(F("MOCK MODE: Simulating ultrasonic sensor hardware."));
    _currentDistance = 1500; // Seed starting value
    return;
  }
  
  _serialPort.begin(baudRate);
  Serial.print(F("Warming up ultrasonic sensor... "));
  
  // Wait until we get a valid reading
  while (_currentDistance == 0) {
    update();
    delay(5);
  }
  
  Serial.println(F("Locked! No 0mm blind spots."));
}

void UltrasonicSensor::update() {
  if (_mockMode) {
    // Automatically generate a highly realistic fluctuating water level distance
    _currentDistance = random(1200, 2000);
    return;
  }

  if (_serialPort.available() > 0) {
    if (_serialPort.peek() != 0xFF) {
      _serialPort.read(); // Discard trailing garbage byte
    } else if (_serialPort.available() >= 4) {
      uint8_t h = _serialPort.read(); // Header: 0xFF
      uint8_t dH = _serialPort.read();
      uint8_t dL = _serialPort.read();
      uint8_t cs = _serialPort.read();

      if (((h + dH + dL) & 0xFF) == cs) {
        uint16_t dist = (dH << 8) | dL;
        if (dist > 0) {
          _currentDistance = dist;
        }
      }
    }
  }
}

uint16_t UltrasonicSensor::getDistance() const {
  return _currentDistance;
}

bool UltrasonicSensor::isMockMode() const {
  return _mockMode;
}
