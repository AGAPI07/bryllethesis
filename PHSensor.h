#ifndef PHSENSOR_H
#define PHSENSOR_H

#include <Arduino.h>

class PHSensor {
  public:
    // Constructor with default values
    PHSensor(
      int sensorPin = 35,
      int numSamples = 20,
      int rawVinegar = 400,
      int rawWater = 2100,
      float pHVinegar = 2.5,
      float pHWater = 7.0,
      unsigned long sampleInterval = 50
    );

    void begin();
    void update();
    float getPH();

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

    float mapPH(float rawValue);
};

#endif
