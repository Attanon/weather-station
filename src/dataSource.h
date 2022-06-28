#ifndef DATASOURCE_H
#define DATASOURCE_H
#endif //DATASOURCE_H

#include <HardwareSerial.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "logging.h"
#include <CertStoreBearSSL.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESP8266HTTPClient.h>

#ifndef URL_HOST
#error("You have to define URL_HOST")
#endif

#ifndef URL_PATH
#error("You have to define URL_PATH")
#endif

class DataSource {

public:
    void init(String* roomName, String* probeName);

    void sendTemperatureData(float temperature, float humidity, float preasure, float altitude);

private:
    const char* sslFingerPrint;
    String* roomName;
    String* probeName;
    BearSSL::CertStore certStore;
    WiFiClientSecure wifiClient;
    bool initialized = false;
};