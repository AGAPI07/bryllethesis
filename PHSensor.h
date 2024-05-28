#ifndef PHSENSOR_H
#define PHSENSOR_H

#include <Arduino.h>

class PHSensor {
public:
  // Constructor with default values
  PHSensor(
    int sensorPin = 35,
    int numSamples = 20,
    int rawVinegar = 450,
    int rawWater = 2200,
    float pHVinegar = 2.5,
    float pHWater = 6.5,
    unsigned long sampleInterval = 30);

  void begin();
  void update();
  float getPH();
  // Method to set the callback
  void setPHCallback(void (*callback)(float));
private:
  int sensorPin;
  int numSamples;
  int rawVinegar;
  int rawWater;
  float pHVinegar;
  float pHWater;
  unsigned long sampleInterval;

  unsigned long lastSampleTime;
  int *rawValues;
  float *phValues;
  int sampleIndex;
  int phIndex;
  float calcPH;

  // Function pointer for the callback
  void (*callback)(float);
  float mapPH(float rawValue);
};

#endif
