#ifndef FIREBASEUPDATER_H
#define FIREBASEUPDATER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

class FirebaseUpdater {
public:
    FirebaseUpdater(const String& lightNode, const String& pHNode, unsigned long intervalMillis = 1000);

    void begin(const String& apiKey, const String& userEmail, const String& userPassword, const String& databaseUrl);
    void update();

private:
    String lightNodePath;
    String pHNodePath;
    unsigned long updateInterval;
    unsigned long previousMillis;
    DefaultNetwork network;
    UserAuth* userAuth;
    FirebaseApp app;
    WiFiClientSecure sslClient;
    AsyncClient aClient;
    RealtimeDatabase database;
    AsyncResult aResult_no_callback;

    void printResult(AsyncResult &aResult);
};

#endif // FIREBASEUPDATER_H
