#include "FirebaseUpdater.h"

FirebaseUpdater::FirebaseUpdater(const String& lightNode, const String& pHNode, unsigned long intervalMillis)
    : lightNodePath(lightNode), pHNodePath(pHNode), updateInterval(intervalMillis), previousMillis(0), aClient(sslClient, getNetwork(network)) {}

void FirebaseUpdater::begin(const String& apiKey, const String& userEmail, const String& userPassword, const String& databaseUrl) {
    Serial.println("Initializing Firebase...");

    userAuth = new UserAuth(apiKey.c_str(), userEmail.c_str(), userPassword.c_str());

    sslClient.setInsecure();

    initializeApp(aClient, app, getAuth(*userAuth), aResult_no_callback);

    app.getApp<RealtimeDatabase>(database);

    database.url(databaseUrl.c_str());
}

void FirebaseUpdater::update() {
    app.loop();
    database.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= updateInterval) {
        previousMillis = currentMillis;

        Serial.println("Updating Firebase...");

        int lightValue = analogRead(34); // Example analog read for light brightness
        float pHValue = analogRead(35) * (3.3 / 4095.0); // Example analog read for pH value

        database.set<int>(aClient, lightNodePath, lightValue, aResult_no_callback);
        database.set<float>(aClient, pHNodePath, pHValue, aResult_no_callback);
    }

    printResult(aResult_no_callback);
}

void FirebaseUpdater::printResult(AsyncResult &aResult) {
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
