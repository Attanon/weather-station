#include "LittleFS.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ArduinoJson.h>
#include "fetch.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <SPI.h>

Adafruit_SHT31 sht31 = Adafruit_SHT31();

WiFiServer server(80);

const char* host = "managing.jakubvrchota.cz";
const int httpsPort = 443;
String url = "/api/v1/public/ingest-data/room-data";
String msg = "";

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

//flag for saving data
bool shouldSaveConfig = false;

String roomName = "roomName";
String probeName = "probeName";

const char* fingerprint = "19 D9 C0 8A C3 F0 EC 4C A1 CD BD 85 02 B8 2F 06 70 32 4C A5";


//callback notifying us of the need to save config
void saveConfigCallback () {
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

WiFiClientSecure client;

bool canMeasureHumidity = true;
bool sendData = false;
int delayTime = 5000; // 60000;

#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BMP3XX bmp;

bool canbpm = true;

void initConfig() {
    //read configuration from FS json
    Serial.println("mounting FS...");

    if (LittleFS.begin()) {
        Serial.println("mounted file system");
        if (LittleFS.exists("/config.json")) {
            //file exists, reading and loading
            Serial.println("reading config file");
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                Serial.println("opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, buf.get());
                if (error) {
                    Serial.println("failed to load json config");
                } else {
                    Serial.println("\nparsed json");
                    roomName = String(doc["roomName"]);
                    probeName = String(doc["probeName"]);
                }
            }
        }
    } else {
        Serial.println("failed to mount FS");
    }

    WiFiManagerParameter custom_room_name("room_name", "roomName", roomName.c_str(), 25);
    WiFiManagerParameter custom_probe_name("probe_name", "probeName", probeName.c_str(), 25);

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    wm.addParameter(&custom_room_name);
    wm.addParameter(&custom_probe_name);

    wm.setSaveConfigCallback(saveConfigCallback);

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("AutoConnectAP"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
    }

    roomName = String(custom_room_name.getValue());
    probeName = String(custom_probe_name.getValue());
}

void sendDataToApi(float t, float h) {
    Serial.print("connecting to : '");
    Serial.print(host);
    Serial.println("'");
    Serial.printf("Using fingerprint '%s'\n", fingerprint);
    client.setFingerprint(fingerprint);

    client.connect(host, httpsPort);

    Serial.print("requesting URL: '");
    Serial.print(url);
    Serial.println("'");

    DynamicJsonDocument doc(1024);
    doc["room"] = roomName;
    doc["probe_name"] = probeName;
    doc["temperature"] = t;
    doc["humidity"] = h;
    serializeJson(doc, Serial);
    Serial.println("");
    serializeJson(doc, msg);

    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n" +
                 "Content-Type: application/json" + "\r\n" +
                 "Content-Length: " + msg.length() + "\r\n" +
                 "\r\n" +
                 msg + "\r\n");

    msg = "";
    Serial.println("request sent");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    //save the custom parameters to FS
    if (shouldSaveConfig) {
        Serial.println("saving config");
        DynamicJsonDocument doc(1024);
        doc["roomName"] = roomName;
        doc["probeName"] = probeName;
        serializeJson(doc, Serial);

        File configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            Serial.println("failed to open config file for writing");
        }

        serializeJson(doc, Serial);
        serializeJson(doc, configFile);
        configFile.close();
        //end save
    }
}

void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);

    while (!Serial)
        delay(10);

    //clean FS, for testing
    //SPIFFS.format();

    initConfig();

    Serial.println("SHT31 test");
    if (! sht31.begin(SHT31_DEFAULT_ADDR)) {   // Set to 0x45 for alternate i2c addr
        Serial.println("Nemohu najít SHT31");
        canMeasureHumidity = false;
    }

    if (!bmp.begin_I2C()) {   // hardware I2C mode, can pass in address & alt Wire
        //if (! bmp.begin_SPI(BMP_CS)) {  // hardware SPI mode
        //if (! bmp.begin_SPI(BMP_CS, BMP_SCK, BMP_MISO, BMP_MOSI)) {  // software SPI mode
        Serial.println("Could not find a valid BMP3 sensor, check wiring!");
        canbpm = false;
    }

    if (canbpm) {
        bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
        bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
        bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
        bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    }

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //or 0x3C
    display.display();
    delay(2000);

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("Connecting ssss");
    display.display();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        EspClass::reset();
    }

    float t = 0;
    float h = 0;

    if (canMeasureHumidity) {
        t = sht31.readTemperature();
        h = sht31.readHumidity();

        if (! isnan(t)) {  // check if 'is not a number'
            Serial.print("Teplota *C = "); Serial.println(t);
        } else {
            Serial.println("Čtení teploty se nezdařilo");
        }

        if (! isnan(h)) {  // check if 'is not a number'
            Serial.print("Vlhkost. % = "); Serial.println(h);
        } else {
            Serial.println("Čtení vlhkosti se nezdařilo");
        }
    }

    if (canbpm && bmp.performReading()) {
        Serial.print("Temperature = ");
        Serial.print(bmp.temperature);
        Serial.println(" *C");

        Serial.print("Pressure = ");
        Serial.print(bmp.pressure / 100.0);
        Serial.println(" hPa");

        Serial.print("Approx. Altitude = ");
        Serial.print(bmp.readAltitude(SEALEVELPRESSURE_HPA));
        Serial.println(" m");
    }

    Serial.println("probeName: " + probeName);
    Serial.println("roomName: " + roomName);
    Serial.println();

//    display.clearDisplay();
//    display.setTextSize(2);
//    display.setTextColor(WHITE);
//    display.setCursor(0,0);
//    display.println("Wifi connected");
//    display.printf("Temp: %f ˚C\n", t);
//    display.printf("Humidity: %f %%\n", h);
//    display.display();

    if (sendData) {
        sendDataToApi(t, h);
    }

    Serial.println("fetching google");
    fetch.GET("https://www.google.com");
    while (fetch.busy())
    {
        if (fetch.available()) {
            Serial.write(fetch.read());
        }

        delay(10);
    }

    if (fetch.available()) {
        Serial.write(fetch.read());
    }

    Serial.println("cleaning");
    fetch.clean();

    delay(delayTime);
}