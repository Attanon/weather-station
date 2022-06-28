#include "dataSource.h"

void DataSource::sendTemperatureData(float temperature, float humidity, float preasure, float altitude) {

    if (this->initialized == false) {
        DEBUG_PROGRAM_LN("SSL certs not working");
        return;
    }

    String msg = "";
    DEBUG_PROGRAM_LN("connecting to : '" + String(URL_HOST) + "'");
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
    HTTPClient httpClient;
    httpClient.begin(wifiClient, String(URL_HOST) + String(URL_PATH));
    serializeJson(doc, msg);
    int respCode = httpClient.POST(msg);

//    wifiClient.print(String("POST ") + URL_PATH + " HTTP/1.1\r\n" +
//                 "Host: " + URL_HOST + "\r\n" +
//                 "Connection: close\r\n" +
//                 "Content-Type: application/json" + "\r\n" +
//                 "Content-Length: " + msg.length() + "\r\n" +
//                 "\r\n" +
//                 msg + "\r\n");

    msg = "";
    DEBUG_PROGRAM_LN("request sent");

    if (respCode == 200) {
        DEBUG_PROGRAM_LN("upload was successful");
    } else {
        DEBUG_PROGRAM_LN("Something wen\'t wrong with request");
    }
//    unsigned long timeout = millis();
//    while (wifiClient.available() == 0) {
//        if (millis() - timeout > 5000) {
//            DEBUG_PROGRAM_LN(">>> Client Timeout !");
//            wifiClient.stop();
//            return;
//        }
//    }

    httpClient.end();
}

void DataSource::init(String *roomName, String *probeName) {
    this->roomName = roomName;
    this->probeName = probeName;

    int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    DEBUG_PROGRAM_F(PSTR("%lu: read %d CA certs into store\r\n"), millis(), numCerts);
    if (numCerts == 0) {
        DEBUG_PROGRAM_LN(F("!!! No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory?"));
    } else {
        wifiClient.setCertStore(&certStore);
        this->initialized = true;
    }
}

