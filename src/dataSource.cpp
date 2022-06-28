#include "dataSource.h"

void DataSource::sendTemperatureData(float temperature, float humidity, float preasure, float altitude) {

    String msg = "";
    WiFiClientSecure client;
    DEBUG_PROGRAM_LN("connecting to : '" + String(URL_HOST) + "'");
    DEBUG_PROGRAM_F("Using fingerprint '%s'\n", this->sslFingerPrint);
    client.setFingerprint(this->sslFingerPrint);

    client.connect(URL_HOST, URL_PORT);

    DEBUG_PROGRAM_LN("requesting URL: '" + String(URL_PATH) + "'");

    DynamicJsonDocument doc(1024);
    doc["room"] = *roomName;
    doc["probe_name"] = *probeName;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["preasure"] = preasure;
    doc["altitude"] = altitude;
#ifdef DEBUG
    serializeJson(doc, Serial);
#endif

    DEBUG_PROGRAM_LN("");
    serializeJson(doc, msg);

    client.print(String("POST ") + URL_PATH + " HTTP/1.1\r\n" +
                 "Host: " + URL_HOST + "\r\n" +
                 "Connection: close\r\n" +
                 "Content-Type: application/json" + "\r\n" +
                 "Content-Length: " + msg.length() + "\r\n" +
                 "\r\n" +
                 msg + "\r\n");

    msg = "";
    DEBUG_PROGRAM_LN("request sent");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            DEBUG_PROGRAM_LN(">>> Client Timeout !");
            client.stop();
            return;
        }
    }
}

void DataSource::init(const char *fingerPrint, String *roomName, String *probeName) {
    this->sslFingerPrint = fingerPrint;
    this->roomName = roomName;
    this->probeName = probeName;
}

