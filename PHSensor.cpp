#include "PHSensor.h"
#include <algorithm>  // For std::sort

PHSensor::PHSensor(
  int sensorPin,
  int numSamples,
  int rawVinegar,
  int rawWater,
  float pHVinegar,
  float pHWater,
  unsigned long sampleInterval) {
  this->sensorPin = sensorPin;
  this->numSamples = numSamples;
  this->rawVinegar = rawVinegar;
  this->rawWater = rawWater;
  this->pHVinegar = pHVinegar;
  this->pHWater = pHWater;
  this->sampleInterval = sampleInterval;
  this->rawValues = new int[numSamples];
  this->phValues = new float[20];
  this->sampleIndex = 0;
  this->phIndex = 0;
  this->calcPH = 7.5;
  this->lastSampleTime = 0;
  this->callback = nullptr;  // Initialize the callback to null
}

void PHSensor::begin() {
  pinMode(sensorPin, INPUT);
}

void PHSensor::update() {
  if (millis() - lastSampleTime >= sampleInterval) {
    lastSampleTime = millis();
    rawValues[sampleIndex] = analogRead(sensorPin);
    sampleIndex++;

    if (sampleIndex >= numSamples) {
      std::sort(rawValues, rawValues + numSamples);

      int total = 0;
      for (int i = 3; i < numSamples - 3; i++) {
        total += rawValues[i];
      }
      float average = total / float(numSamples - 6);

      float pHValue = mapPH(average);
      float constrainedPH = constrain(pHValue, 0, 14);

      if (phIndex < 20) {
        phValues[phIndex] = constrainedPH;
        phIndex++;
      } else {
        std::sort(phValues, phValues + 20);

        float totalPH = 0;
        for (int i = 3; i < 20 - 3; i++) {
          totalPH += phValues[i];
        }
        float averagePH = totalPH / float(20 - 6);

        calcPH = constrain(averagePH, 0, 14);
        Serial.print("Final pH: ");
        Serial.println(calcPH);
        if (callback) {
          callback(calcPH);
        }
        phIndex = 0;
      }

      sampleIndex = 0;
    }
  }
}

float PHSensor::getPH() {
  return calcPH;
}

float PHSensor::mapPH(float rawValue) {
  float pHValue = pHVinegar + (rawValue - rawVinegar) * (pHWater - pHVinegar) / (rawWater - rawVinegar);
  return pHValue;
}
void PHSensor::setPHCallback(void (*callback)(float)) {
  this->callback = callback;
}
