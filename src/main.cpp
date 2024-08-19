//https://randomnerdtutorials.com/esp32-sim800l-publish-data-to-cloud/
//https://randomnerdtutorials.com/guide-to-neo-6m-gps-module-with-arduino/
//https://github.com/vshymanskyy/TinyGSM/blob/master/examples/HttpsClient/HttpsClient.ino
//https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/

#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.

#include "AdvancedConfig.hpp"

#ifdef BATTERY_DISABLED_PIN
#ifndef BATTERY_UPDATE_INTERVAL
#error "Please define the update interval for when on battery power."
#endif
#define BATTERY_UPDATE_INTERVAL_US BATTERY_UPDATE_INTERVAL * 1000000
#endif

#include <freertos/task.h>
#include <Wire.h>
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>
#include <HttpClient.h>
#include <vector>
#include <chrono>
#include <TinyGPSPlus.h>
#ifdef _DEBUG
  #include <StreamDebugger.h>
#endif

#define UPDATE_INTERVAL_US UPDATE_INTERVAL * 1000000

#define StdSerial Serial
#define ModemSerial Serial1
SoftwareSerial GpsSerial(GPS_RX, GPS_TX);
#ifdef _DEBUG
  StreamDebugger ModemDebugger(ModemSerial, StdSerial);
  TinyGsm Modem(ModemDebugger);
  StreamDebugger GpsDebugger(GpsSerial, StdSerial);
#else
  TinyGsm Modem(ModemSerial);
#endif

TwoWire ModemI2c = TwoWire(0);
#if SERVER_SSL
    TinyGsmClientSecure GsmClient(Modem);
#else
    TinyGsmClient GsmClient(Modem);
#endif
HttpClient NetClient(GsmClient, SERVER_FQDN, SERVER_PORT);

TinyGPSPlus Gps;

void setup()
{
    pinMode(MODEM_PWKEY, OUTPUT);
    pinMode(MODEM_RST, OUTPUT);
    pinMode(MODEM_PWRON, OUTPUT);
    digitalWrite(MODEM_PWKEY, LOW);
    digitalWrite(MODEM_RST, HIGH);
    digitalWrite(MODEM_PWRON, HIGH);

    StdSerial.begin(115200);

    ModemI2c.begin(MODEM_SDA, MODEM_SDL, 400000);
    ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    Modem.restart();

    #ifdef MODEM_PIN
    if (strlen(MODEM_PIN) && Modem.getSimStatus() != 3)
        Modem.simUnlock(MODEM_PIN);
    #endif

    #ifdef BATTERY_DISABLED_PIN
    pinMode(BATTERY_DISABLED_PIN, INPUT);
    uint64_t batteryPinMask = 1ULL << BATTERY_DISABLED_PIN;
    esp_deep_sleep_enable_gpio_wakeup(BATTERY_DISABLED_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_sleep_enable_timer_wakeup(digitalRead(BATTERY_DISABLED_PIN) == LOW ? BATTERY_UPDATE_INTERVAL_US : UPDATE_INTERVAL_US);
    #else
    esp_sleep_enable_timer_wakeup(UPDATE_INTERVAL_US);
    #endif
}

std::vector<String> SplitString(const String &data, char delimiter)
{
    std::vector<String> result;
    int start = 0;
    int end = data.indexOf(delimiter);

    while (end != -1)
    {
        result.push_back(data.substring(start, end));
        start = end + 1;
        end = data.indexOf(delimiter, start);
    }

    // Add the last part of the string
    result.push_back(data.substring(start));

    return result;
}

#ifdef ARDUINO
void loop()
{
    switch(esp_sleep_get_wakeup_cause())
    {
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
        default: break;
    }

    String serverData = SERVER_DATA_FORMAT;

    StdSerial.print("Obtaining GPS data...");
    ulong start = millis();
    while (!Gps.location.isUpdated() && GpsSerial.available() > 0 && millis() - start < std::chrono::seconds(10).count())
        Gps.encode(GpsSerial.read());
    if (!Gps.location.isUpdated())
    {
        StdSerial.println(" | FAILED");
        goto sleep;
    }

    StdSerial.print("Connecting to APN: ");
    StdSerial.print(MODEM_APN);
    if (!Modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
    {
        StdSerial.println(" | FAILED");
        goto sleep; //TODO: Retry.
    }

    StdSerial.print("Waiting for network...");
    if (!Modem.waitForNetwork())
    {
        StdSerial.println(" | FAILED");
        goto sleep;
    }

    StdSerial.print("Performing network request... ");
    NetClient.connectionKeepAlive(); //Currently, this is needed for HTTPS.

    NetClient.beginRequest();
    #ifdef SERVER_HEADER_AUTHORIZATION
        NetClient.sendHeader("Authorization", SERVER_HEADER_AUTHORIZATION);
    #endif
    #ifdef SERVER_ADDITIONAL_HEADERS
    //Seperate by CLRF.
    std::vector<String> headers = SplitString(SERVER_ADDITIONAL_HEADERS, '\n');
    for (size_t i = 0; i < headers.size(); i++)
    {
        std::vector<String> header = SplitString(headers[i], ':');
        if (header.size() != 2)
            continue;
        NetClient.sendHeader(header[0], header[1]);
    }
    
    #endif

    //Replace placeholders.
    serverData.replace("{{longitude}}", String(Gps.location.lng(), 6));
    serverData.replace("{{latitude}}", String(Gps.location.lat(), 6));
    serverData.replace("{{altitude}}", String(Gps.altitude.meters(), 2));
    serverData.replace("{{accuracy}}", String(Gps.hdop.hdop(), 2));

    int err;
    #if SERVER_METHOD == 1 //POST.
        //Get content type from headers.
        err = NetClient.post(SERVER_PATH, SERVER_HEADER_CONTENT_TYPE, serverData);
    #else
    #error "Server method not implemented."
    #endif
    NetClient.endRequest();
    if (err != 0 || NetClient.responseStatusCode() != 200)
    {
        StdSerial.println(" | FAILED");
        goto sleep;
    }

    StdSerial.println(" | SUCCESS");

sleep: //TODO: Temporary.
    GsmClient.stop();
    Modem.gprsDisconnect();

    #ifdef BATTERY_DISABLED_PIN
    esp_sleep_enable_timer_wakeup(digitalRead(BATTERY_DISABLED_PIN) == LOW ? BATTERY_UPDATE_INTERVAL_US : UPDATE_INTERVAL_US);
    #endif
    StdSerial.println("Entering standby...");
    esp_deep_sleep_start();
}
#else
extern "C" void app_main()
{
    setup();
    for (;;)
        loop();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
