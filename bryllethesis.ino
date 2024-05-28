#include <Arduino.h>
#include "AnalogReader.h"
#include <ESP32Firebase.h>
#include "PHSensor.h"

// Create an instance of PHSensor with default values
PHSensor phSensor;

#define _SSID "IVILLIS 2.4G"                                                                    // Your WiFi SSID
#define _PASSWORD "akogwapo123"                                                                 // Your WiFi Password
#define REFERENCE_URL "https://hydro-8aaba-default-rtdb.asia-southeast1.firebasedatabase.app/"  // Your Firebase project reference url
Firebase firebase(REFERENCE_URL);

/* PINS */
const int inputPin = 35;        // Input pin for the sensor
const int drainPin = 23;        // Output pin for draining
const int refillPin = 22;       // Output pin for refilling
const int cyclePin = 21;        // Output pin for cycle control
const int lightSensorPin = 34;  // Input for daylight sensor
const int pwmPin = 16;          // PWM pin connected to the gate of the MOSFET

/* Delays */
const unsigned long drainTime = 60000;   // Time for draining in milliseconds
const unsigned long refillTime = 60000;  // Time for refilling in milliseconds
const unsigned long readDelay = 7000;    // Delay before reading pH again
const int drainThresh = 4;               // Threshold for draining
unsigned long lastLightSensorTime = 0;
const unsigned long lightSensorDelay = 1000;

/* PWM Properties */
const int pwmChannel = 0;
const int pwmFrequency = 5000;  // 5 kHz
const int pwmResolution = 8;    // 8-bit resolution (values between 0 and 255)
const float sensitivity = 5;    // Sensitivity factor for LDR

unsigned long previousTime = 0;  // To store the last recorded time
bool isDraining = false;
bool isRefilling = false;
bool canRead = true;

/* PH Reading */
#define NUM_READINGS 30
#define REMOVE_COUNT 5
#define VALID_READINGS (NUM_READINGS - 2 * REMOVE_COUNT)

unsigned long lastReadTime = 0;
int readIndex = 0;
float sensorValues[NUM_READINGS];
bool readingInProgress = false;
float averageValue = 0;

void setup() {
  Serial.begin(115200);
  SetupWifiDatabase();
  // Initialize pins
  pinMode(inputPin, INPUT);
  pinMode(lightSensorPin, INPUT);
  pinMode(drainPin, OUTPUT);
  pinMode(refillPin, OUTPUT);
  pinMode(cyclePin, OUTPUT);

  // Configure the PWM functionality of the pin
  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);

  // Start with the LED off
  ledcWrite(pwmChannel, 0);

  phSensor.begin();
}

void SetupWifiDatabase() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  // Connect to WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(_SSID);
  WiFi.begin(_SSID, _PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("-");
  }

  Serial.println("");
  Serial.println("WiFi Connected");

  // Print the IP address
  Serial.print("IP Address: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  digitalWrite(cyclePin, HIGH);
}

float mapToPH(float rawValue, float rawAcidicMin, float rawAlkalineMax, float pHMin = 0.0, float pHMax = 14.0) {
  return ((rawValue - rawAcidicMin) * (pHMax - pHMin)) / (rawAlkalineMax - rawAcidicMin) + pHMin;
}

float processSensorValues() {
  // Sort the sensor values
  for (int i = 0; i < NUM_READINGS - 1; i++) {
    for (int j = 0; j < NUM_READINGS - i - 1; j++) {
      if (sensorValues[j] > sensorValues[j + 1]) {
        float temp = sensorValues[j];
        sensorValues[j] = sensorValues[j + 1];
        sensorValues[j + 1] = temp;
      }
    }
  }

  // Calculate the average of the middle 20 values
  float sum = 0;
  for (int i = REMOVE_COUNT; i < NUM_READINGS - REMOVE_COUNT; i++) {
    sum += sensorValues[i];
  }
  return sum / VALID_READINGS;
}

float readSensorValues() {
  unsigned long currentMillis = millis();

  if (!readingInProgress) {
    readingInProgress = true;
    lastReadTime = currentMillis;
    readIndex = 0;
  }

  if (currentMillis - lastReadTime >= 100) {
    lastReadTime = currentMillis;

    if (readIndex < NUM_READINGS) {
      sensorValues[readIndex] = analogRead(inputPin);
      // Serial.println("pH Value: " + String(sensorValues[readIndex]));
      // firebase.setInt("Example/pHValue", sensorValues[readIndex]);
      readIndex++;
    }

    if (readIndex == NUM_READINGS) {
      readingInProgress = false;
      return processSensorValues();
    }
  }

  return -1;  // Indicate that reading is still in progress
}

void pH_Read() {
  phSensor.update();
  float avgValue = phSensor.getPH();

  
  if (!canRead) return;
  if (avgValue <= drainThresh) {
    drain();
  }
  // delay(500);
}

int getBrightnessFromLDR() {
  int ldrValue = analogRead(lightSensorPin);  // Read the analog value (0 - 4095)
  ldrValue = ldrValue * sensitivity;          // Apply sensitivity factor
  ldrValue = constrain(ldrValue, 0, 4095);    // Ensure the adjusted value stays within the valid range

  // Inversely map it to PWM range (0 - 255)
  int brightness = map(ldrValue, 0, 4095, 255, 0);
  return brightness;
}

void lightSensor() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastLightSensorTime >= lightSensorDelay) {
    lastLightSensorTime = currentMillis;
    int brightness = getBrightnessFromLDR();
    ledcWrite(pwmChannel, brightness);

    // Print the values for debugging
    // firebase.setInt("Example/Light Brightness", brightness);
    Serial.print("LDR Value: ");
    Serial.print(analogRead(lightSensorPin));
    Serial.print(" -> Brightness: ");
    Serial.println(brightness);
  }
}

// -------------------------------------- LOOP -----------------------------
void loop() {
  lightSensor();
  pH_Read();

  if (isDraining || isRefilling || !canRead) {
    checkProcessTime();
  }

  delay(10);
}

void checkProcessTime() {
  unsigned long currentTime = millis();
  unsigned long deltaTime = currentTime - previousTime;

  if (isDraining && deltaTime >= drainTime) {
    stopDrain();
    refill();
  } else if (isRefilling && deltaTime >= refillTime) {
    stopRefill();
    pHReadDelay();
  } else if (!isDraining && !isRefilling && !canRead && deltaTime >= readDelay) {
    enablePHRead();
  }
}

void drain() {
  digitalWrite(drainPin, HIGH);
  previousTime = millis();
  Serial.println("Draining Water...");
  canRead = false;
  digitalWrite(cyclePin, LOW);
  isDraining = true;
}

void stopDrain() {
  digitalWrite(drainPin, LOW);
  isDraining = false;
  Serial.println("Done Draining.");
}

void refill() {
  digitalWrite(refillPin, HIGH);
  previousTime = millis();
  Serial.println("Refilling Water...");
  isRefilling = true;
}

void stopRefill() {
  digitalWrite(refillPin, LOW);
  isRefilling = false;
  digitalWrite(cyclePin, HIGH);
  Serial.println("Done Refill.");
}

void pHReadDelay() {
  previousTime = millis();
  Serial.println("Water Cycle Returned. Initializing pH read...");
}

void enablePHRead() {
  canRead = true;
  Serial.println("pH Read back online.");
}
