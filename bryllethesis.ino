#include <Arduino.h>
#include <WiFi.h>
#include "PHSensor.h"
// #include <ESP32Firebase.h>
// Create an instance of PHSensor with default values
PHSensor phSensor;

#define _SSID "IVILLIS 2.4G"                                                                    // Your WiFi SSID
#define _PASSWORD "akogwapo123"                                                                 // Your WiFi Password
// #define REFERENCE_URL "https://hydro-8aaba-default-rtdb.asia-southeast1.firebasedatabase.app/"  // Your Firebase project reference url
// Firebase firebase(REFERENCE_URL);

#include <FirebaseClient.h>
// The API key can be obtained from Firebase console > Project Overview > Project settings.
#define API_KEY "AIzaSyCux2ojAMwsaLjhu86BGla9vP9AyBJrjnM"

// User Email and password that already registerd or added in your project.
#define USER_EMAIL "sample@gmail.com"
#define USER_PASSWORD "password"
#define DATABASE_URL "https://hydro-8aaba-default-rtdb.asia-southeast1.firebasedatabase.app/"

void asyncCB(AsyncResult &aResult);

void printResult(AsyncResult &aResult);

DefaultNetwork network;  // initilize with boolean parameter to enable/disable network reconnection

UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);

FirebaseApp app;

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_GIGA) || defined(ARDUINO_PORTENTA_C33) || defined(ARDUINO_NANO_RP2040_CONNECT)
#include <WiFiSSLClient.h>
WiFiSSLClient ssl_client;
#endif

using AsyncClient = AsyncClientClass;

AsyncClient aClient(ssl_client, getNetwork(network));

RealtimeDatabase Database;

AsyncResult aResult_no_callback;

bool taskComplete = false;
/* PINS */
const int inputPin = 35;        // Input pin for the sensor
const int drainPin = 23;        // Output pin for draining
const int refillPin = 22;       // Output pin for refilling
const int cyclePin = 21;        // Output pin for cycle control
const int lightSensorPin = 34;  // Input for daylight sensor
const int pwmPin = 16;          // PWM pin connected to the gate of the MOSFET

/* Delays */
const unsigned long drainTime = 300000;   // Time for draining in milliseconds
const unsigned long refillTime = 180000;  // Time for refilling in milliseconds
const unsigned long readDelay = 7000;     // Delay before reading pH again
const int drainThresh = 5;              // Threshold for draining
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
int Brightness = 255;
float PHVALUE = 7.0;


unsigned long lastReadTime = 0;
bool readingInProgress = false;
float averageValue = 0;

void setup() {
  Serial.begin(115200);
  SetupWifiDatabase();
  firebaseSetup();
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
void firebaseSetup() {

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  Serial.println("Initializing app...");

#if defined(ESP32) || defined(ESP8266) || defined(PICO_RP2040)
  ssl_client.setInsecure();
#if defined(ESP8266)
  ssl_client.setBufferSizes(4096, 1024);
#endif
#endif

  initializeApp(aClient, app, getAuth(user_auth), asyncCB, "authTask");

  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);

  Serial.println("Asynchronous Set... ");
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
  phSensor.setPHCallback(onPHUpdate);
  digitalWrite(cyclePin, HIGH);
}

// --------------------------------------------- CALLBACK

void asyncCB(AsyncResult &aResult) {
  // WARNING!
  // Do not put your codes inside the callback and printResult.

  printResult(aResult);
}

void printResult(AsyncResult &aResult) {
  if (aResult.isEvent()) {
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }

  if (aResult.isDebug()) {
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  }

  if (aResult.isError()) {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  }

  if (aResult.available()) {
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
  }
}
void onPHUpdate(float pHValue) {
  Serial.print("Callback pH Value: ");
  Serial.println(pHValue);
  PHVALUE = pHValue;
  // firebase.setFloat("Example/pHValue", pHValue);
  // firebase.setInt("Example/Light Brightness", Brightness);
  // Perform other actions when pH value is updated
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
    Brightness = brightness;
    ledcWrite(pwmChannel, brightness);

    // Print the values for debugging

    Serial.print("LDR Value: ");
    Serial.print(analogRead(lightSensorPin));
    Serial.print(" -> Brightness: ");
    Serial.println(brightness);
  }
}

// -------------------------------------- LOOP -----------------------------
void firebaseLoop() {
  // The async task handler should run inside the main loop
  // without blocking delay or bypassing with millis code blocks.

  app.loop();

  Database.loop();

  if (app.ready() && !taskComplete) {
    taskComplete = true;

    Serial.println("Asynchronous Set... ");

    // Set int
    Database.set<int>(aClient, "Example/Light Brightness", Brightness, asyncCB, "setIntTask");

    // Set bool
    Database.set<bool>(aClient, "/test/bool", true, asyncCB, "setBoolTask");

    // Set string
    Database.set<String>(aClient, "/test/string", "hello", asyncCB, "setStringTask");

    // Set json
    Database.set<object_t>(aClient, "/test/json", object_t("{\"data\":123}"), asyncCB, "setJsonTask");

    // Library does not provide JSON parser library, the following JSON writer class will be used with
    // object_t for simple demonstration.

    object_t json, obj1, obj2, obj3, obj4;
    JsonWriter writer;

    writer.create(obj1, "int/value", 9999);
    writer.create(obj2, "string/value", string_t("hello"));
    writer.create(obj3, "float/value", number_t(123.456, 2));
    writer.join(obj4, 3 /* no. of object_t (s) to join */, obj1, obj2, obj3);
    writer.create(json, "node/list", obj4);

    // To print object_t
    // Serial.println(json);

    Database.set<object_t>(aClient, "/test/json", json, asyncCB, "setJsonTask");

    object_t arr;
    arr.initArray();  // initialize to be used as array
    writer.join(arr, 4 /* no. of object_t (s) to join */, object_t("[12,34]"), object_t("[56,78]"), object_t(string_t("steve")), object_t(888));

    // Note that value that sets to object_t other than JSON ({}) and Array ([]) can be valid only if it
    // used as array member value as above i.e. object_t(string_t("steve")) and object_t(888).

    // Set array
    Database.set<object_t>(aClient, "/test/arr", arr, asyncCB, "setArrayTask");

    // Set float
    Database.set<number_t>(aClient, "Example/pHValue", number_t(PHVALUE, 2), asyncCB, "setFloatTask");

    // Set double
    Database.set<number_t>(aClient, "/test/double", number_t(1234.56789, 4), asyncCB, "setDoubleTask");
  }
}
void loop() {
  lightSensor();
  pH_Read();
  firebaseLoop();
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
