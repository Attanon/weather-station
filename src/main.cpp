#include "LittleFS.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ArduinoJson.h>
#include "SSD1306Wire.h"
#include "Adafruit_BMP3XX.h"
#include "dataSource.h"
#include "OLEDDisplayUi.h"
#include "time.h"
#include <FS.h>
#include "logging.h"

Adafruit_SHT31 sht31 = Adafruit_SHT31();

//flag for saving data
bool shouldSaveConfig = false;

String roomName = "roomName";
String probeName = "probeName";

bool canMeasureHumidity = true;
bool sendData = true;
int readEverySeconds = 10; // in seconds
int sendEverySeconds = 60; // in seconds

#define OLED_RESET 0
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_64_48);

DataSource dataSource;

#define RESET_WIFI_BUTTON D0
#define NEXT_FRAME_BUTTON D5

#define SEALEVELPRESSURE_HPA (1013.25)

void drawFrameStatistics(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrameProbeWifiConfig(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrameProbeConfig(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);

FrameCallback frames[] = {drawFrameStatistics, drawFrameProbeConfig, drawFrameProbeWifiConfig};

void initConfig();
void saveConfigCallback();
void saveConfig();

// how many frames are there?
int frameCount = std::size(frames);

OLEDDisplayUi ui     ( &display );

Adafruit_BMP3XX bmp;

float temperature = 0;
float humidity = 0;
float preasure = 0;
float altitude = 0;

bool canbpm = true;

time_t startTime;
WiFiManager wm;

bool shouldResetConnection = false;

void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);

    while (!Serial)
        delay(10);

    //read configuration from FS json
    DEBUG_PROGRAM_LN("mounting FS...");

    if (LittleFS.begin()) {
        DEBUG_PROGRAM_LN("mounted file system");
    } else {
        DEBUG_PROGRAM_LN("cannot mounted file system");
        return;
    }

    //clean FS, for testing
    //LittleFS.format();

    pinMode(RESET_WIFI_BUTTON, INPUT);

    display.init();
    display.clear();
    display.flipScreenVertically();
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.setFont(ArialMT_Plain_10);
    display.drawString(25, 15, "Starting...");
    display.display();

    delay(1000);

    initConfig();
    dataSource.init(&roomName, &probeName);

    DEBUG_PROGRAM_LN("test for SHT31 probe (temperature, humidity)");
    if (! sht31.begin(SHT31_DEFAULT_ADDR)) {   // Set to 0x45 for alternate i2c addr
        DEBUG_PROGRAM_LN("Cannot find SHT31");
        canMeasureHumidity = false;
    }

    // bmp probe
    DEBUG_PROGRAM_LN("test for BMP388 probe (preasure and altitude)");
    if (!bmp.begin_I2C()) {   // hardware I2C mode, can pass in address & alt Wire
        DEBUG_PROGRAM_LN("Could not find a valid BMP388 sensor");
        canbpm = false;
    } else {
        bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
        bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
        bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
        bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    }

    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    while (!time(nullptr)) {
        DEBUG_PROGRAM_LN(".");
        delay(1000);
    }

    ui.setTargetFPS(1);
    ui.setFrames(frames, frameCount);
    ui.setIndicatorPosition(BOTTOM);

    // Defines where the first frame is located in the bar.
    ui.setIndicatorDirection(LEFT_RIGHT);

    ui.disableAutoTransition();

    ui.init();
    delay(1000);
    startTime = time(nullptr);
    display.flipScreenVertically();
    DEBUG_PROGRAM_LN("startTime: %s" + String(startTime));
}

bool nextFrame = false;
bool resetConfig = false;
bool readed = false;
bool sentData = false;
double lastSeconds = 0;
bool isFirst = true;

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        EspClass::reset();
    }

    int remainingTimeBudget = ui.update();
    if (remainingTimeBudget > 0) {
        time_t now = time(nullptr);
        DEBUG_PROGRAM_VERBOSE_LN("now: " + String(now));
        DEBUG_PROGRAM_VERBOSE_LN("startTime: " + String(startTime));
        int diff = (int)difftime(now, startTime);
        if (diff != lastSeconds) {
            lastSeconds = diff;
            DEBUG_PROGRAM_VERBOSE_LN("diff in seconds: " + String(diff));
        }

        int button = digitalRead(RESET_WIFI_BUTTON);
        if (button == HIGH) {
            resetConfig = true;
        } else if (resetConfig) {
            resetConfig = false;
            DEBUG_PROGRAM_LN("reseting");
            shouldResetConnection = true;
            roomName = "roomName";
            probeName = "probeName";
            saveConfig();
            ESP.reset();
        }

        int buttonFrame = digitalRead(NEXT_FRAME_BUTTON);
        if (buttonFrame == HIGH) {
            nextFrame = true;
        } else if (nextFrame) {
            ui.nextFrame();
            DEBUG_PROGRAM_LN("moving to next frame");
            nextFrame = false;
        }

        if ((diff % sendEverySeconds) == 0 && diff > 0) {
            if (!sentData) {
                if (sendData) {
                    dataSource.sendTemperatureData(temperature, humidity, preasure, altitude);
                }

                sentData = true;
            }
        } else if (sentData) {
            sentData = false;
        }

        if ((diff % readEverySeconds) == 0 || (isFirst && diff == 2)) {
            if (!readed) {
                isFirst = false;
                temperature = 0;
                humidity = 0;
                preasure = 0;
                altitude = 0;

                if (canMeasureHumidity) {
                    temperature = sht31.readTemperature();
                    humidity = sht31.readHumidity();
                }

                if (canbpm && bmp.performReading()) {
                    double temp = bmp.temperature;

                    if (temperature == 0) {
                        temperature = (float) temp;
                    }

                    double pRead = bmp.pressure / 100.0;

                    if (preasure == 0) {
                        preasure = (float) pRead;
                    }

                    altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
                }

                DEBUG_PROGRAM_F("Room: %s (%s) \n", roomName.c_str(), probeName.c_str());

                DEBUG_PROGRAM_F("Temperature: %f ˚C\n", temperature);
                DEBUG_PROGRAM_F("Humidity: %f %%\n", humidity);
                DEBUG_PROGRAM_F("Preassure: %f hPa\n", preasure);
                DEBUG_PROGRAM_F("Approx. Altitude: %f m\n", altitude);

                readed = true;
            }
        } else if (readed) {
            readed = false;
        }
    }
}

void initConfig() {
    if (LittleFS.exists("/config.json")) {
        //file exists, reading and loading
        DEBUG_PROGRAM_LN("reading config file");
        File configFile = LittleFS.open("/config.json", "r");
        if (configFile) {
            DEBUG_PROGRAM_LN("opened config file");
            size_t size = configFile.size();
            // Allocate a buffer to store contents of the file.
            std::unique_ptr<char[]> buf(new char[size]);

            configFile.readBytes(buf.get(), size);
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, buf.get());
            if (error) {
                DEBUG_PROGRAM_LN("failed to load json config");
            } else {
                DEBUG_PROGRAM_LN("parsed json");
                roomName = String(doc["roomName"]);
                probeName = String(doc["probeName"]);
                shouldResetConnection = (bool)doc["shouldReset"];
            }
        }
    }

    WiFiManagerParameter custom_room_name("room_name", "roomName", roomName.c_str(), 25);
    WiFiManagerParameter custom_probe_name("probe_name", "probeName", probeName.c_str(), 25);

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around

    wm.addParameter(&custom_room_name);
    wm.addParameter(&custom_probe_name);

    wm.setSaveConfigCallback(saveConfigCallback);

    String ssid = "AttanonWeather " + String(ESP.getChipId());
    if (shouldResetConnection || WiFi.SSID() == "") {
        display.clear();
        display.flipScreenVertically();
        display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
        display.setFont(ArialMT_Plain_10);
        display.drawString(30, 15, "Reseting wifi");
        display.drawStringMaxWidth(33, 25, 64, ssid);
        display.display();

        wm.resetSettings();
    }


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
    res = wm.autoConnect(ssid.c_str()); // password protected ap

    if(!res) {
        DEBUG_PROGRAM_LN("Failed to connect");
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi
        DEBUG_PROGRAM_LN("connected...yeey :)");

        shouldResetConnection = false;
        saveConfig();
    }

    roomName = String(custom_room_name.getValue());
    probeName = String(custom_probe_name.getValue());

    if (shouldSaveConfig) {
        saveConfig();
    }
}

void saveConfig() {
    DEBUG_PROGRAM_LN("saving config");
    DynamicJsonDocument doc(1024);
    doc["roomName"] = roomName;
    doc["probeName"] = probeName;
    doc["shouldReset"] = shouldResetConnection;
#ifdef DEBUG
    serializeJson(doc, Serial);
#endif

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
        DEBUG_PROGRAM_LN("failed to open config file for writing");
    }

#ifdef DEBUG
    serializeJson(doc, Serial);
#endif
    serializeJson(doc, configFile);
    configFile.close();
}

//callback notifying us of the need to save config
void saveConfigCallback() {
    DEBUG_PROGRAM_LN("Should save config");
    shouldSaveConfig = true;
}

void drawFrameStatistics(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
    char preasureString[16];
    sprintf(preasureString, "%.1f hPa", preasure);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0 + x,0 + y, "T " + (String)temperature + " ˚C");
    display->drawString(0 + x , 10 + y, "H " + (String)humidity + " %");
    display->drawString(0 + x, 20 + y, "Pr " + String(preasureString));
    display->drawString(0 + x, 30 + y, "Alt " + (String)altitude + " m");
}

void drawFrameProbeConfig(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawStringMaxWidth(0 + x, 0 + y, 64, "Room: ");
    display->drawStringMaxWidth(0 + x, 10 + y, 64, roomName);
    display->drawStringMaxWidth(0 + x, 20 + y, 64, "Probe name: ");
    display->drawStringMaxWidth(0 + x, 30 + y, 64, probeName);
}

void drawFrameProbeWifiConfig(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawStringMaxWidth(0 + x, 0 + y, 64, "Wifi: ");
    display->drawStringMaxWidth(0 + x, 10 + y, 64, WiFi.SSID());
}