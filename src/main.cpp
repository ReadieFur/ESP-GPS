//https://randomnerdtutorials.com/esp32-sim800l-publish-data-to-cloud/
//https://randomnerdtutorials.com/guide-to-neo-6m-gps-module-with-arduino/
//https://github.com/vshymanskyy/TinyGSM/blob/master/examples/HttpsClient/HttpsClient.ino
//https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/

// #define ENABLE_FLASH_SETTINGS

#include "Helpers.h"
#include "Config.h"
#include "stdSerial.hpp"
#include <StreamDebugger.h>
#include <SoftwareSerial.h>
#include <TinyGSM.h>
#include <TinyGPSPlus.h>
#ifdef ENABLE_FLASH_SETTINGS
#include "Settings.hpp"
#endif
#include <chrono>
#include <HttpClient.h>
#include <vector>

#ifdef BATTERY_DISABLED_PIN
#ifndef BATTERY_UPDATE_INTERVAL
#error "Please define the update interval for when on battery power."
#endif
#endif
#if SERVER_METHOD < 1 || SERVER_METHOD > 1
#error "Server method not implemented."
#endif

#ifdef ENABLE_SMS_API
#error "SMS API not yet implemented."
#endif

#ifdef SERVER_SSL
#error "SSL not yet supported."
#endif

#if defined(ESP32)
#define Delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#else
#define Delay(ms) delay(ms)
#endif

SoftwareSerial modemSerial(MODEM_RX, MODEM_TX);
#ifdef _DEBUG
StreamDebugger _modemDebugger(modemSerial, stdSerial);
TinyGsm modem(_modemDebugger);
#else
TinyGsm Modem(modemSerial);
#endif
TinyGsmClient* gsmClient = nullptr;

SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
#ifdef _DEBUG
StreamDebugger _gpsDebugger(gpsSerial, stdSerial);
#endif
TinyGPSPlus gps;

void InitIO()
{
    stdSerial.begin(115200);

    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
    #ifdef MODEM_DTR
    pinMode(MODEM_DTR, OUTPUT);
    digitalWrite(MODEM_DTR, LOW);
    #endif
    #ifdef MODEM_RING
    pinMode(MODEM_RING, OUTPUT);
    digitalWrite(MODEM_RING, LOW);
    #endif
}

bool InitModem()
{
    //Begin communication with the modem.
    modemSerial.begin(115200, SWSERIAL_8N1);
    modem.restart();

    #ifdef MODEM_PIN
    //Unlock the SIM.
    if (strlen(MODEM_PIN)
        && Modem.getSimStatus() != 3
        && !Modem.simUnlock(MODEM_PIN));
    {
        stdSerial.println("Failed to unlock SIM card.");
        abort();
    }
    #endif

    #ifdef ENABLE_FLASH_SETTINGS
    gsmClient = settings.Get<bool>(nameof(SERVER_SSL))
        ? new TinyGsmClientSecure(Modem)
        : new TinyGsmClient(Modem);
    #else
    gsmClient = new TinyGsmClient(modem);
    #endif

    stdSerial.print("Connecting to APN: ");
    stdSerial.print(MODEM_APN);
    if (!modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
    {
        stdSerial.println(" | FAILED");
        return false;
    }

    return true;
}

void InitGps()
{
    gpsSerial.begin(9600);
}

void ConfigureSleep()
{
    uint64_t sleepDuration = 0;
    uint64_t gpioMask = 0;

    #ifdef MODEM_RING
    pinMode(MODEM_RING, INPUT);

    #if defined(ESP32)
    if (!esp_sleep_is_valid_wakeup_gpio(MODEM_RING))
    {
        stdSerial.println(String("Invalid GPIO for ") + nameof(MODEM_RING));
        abort();
    }
    #endif
    gpioMask |= BIT(MODEM_RING);
    #endif

    #ifdef BATTERY_DISABLED_PIN
    pinMode(BATTERY_DISABLED_PIN, INPUT);
    #if defined(ESP32)
    if (!esp_sleep_is_valid_wakeup_gpio(BATTERY_DISABLED_PIN))
    {
        stdSerial.println(String("Invalid GPIO for ") + nameof(BATTERY_DISABLED_PIN));
        abort();
    }
    #endif
    gpioMask |= BIT(BATTERY_DISABLED_PIN);
    #ifdef ENABLE_FLASH_SETTINGS
    sleepDuration = Settings.getULong(digitalRead(BATTERY_DISABLED_PIN) == LOW ? nameof(BATTERY_UPDATE_INTERVAL) : nameof(UPDATE_INTERVAL));
    #else
    sleepDuration = digitalRead(BATTERY_DISABLED_PIN) == LOW ? BATTERY_UPDATE_INTERVAL : UPDATE_INTERVAL;
    #endif
    #else
    #ifdef ENABLE_FLASH_SETTINGS
    sleepDuration = Settings.getULong(nameof(UPDATE_INTERVAL));
    #else
    sleepDuration = UPDATE_INTERVAL;
    #endif
    #endif

    #if defined(ESP32)
    esp_sleep_enable_timer_wakeup(sleepDuration * 1000000);
    esp_deep_sleep_enable_gpio_wakeup(BIT(MODEM_RING), ESP_GPIO_WAKEUP_GPIO_HIGH);
    #endif
}

bool GetGps()
{
    stdSerial.print("Obtaining GPS data...");
    ulong start = millis();
    while (!gps.location.isUpdated() && gpsSerial.available() > 0 && millis() - start < std::chrono::seconds(2).count())
        gps.encode(gpsSerial.read());

    if (!gps.location.isUpdated())
    {
        stdSerial.println(" | FAILED");
        return false;
    }

    return true;
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

bool PublishData()
{
    stdSerial.print("Waiting for network...");
    if (!modem.waitForNetwork())
    {
        stdSerial.println(" | FAILED");
        return false;
    }

    #ifdef ENABLE_FLASH_SETTINGS
    HttpClient httpClient(*gsmClient,
        Settings.getString(nameof(SERVER_FQDN)),
        Settings.getUShort(nameof(SERVER_PORT)));
    String serverData = Settings.getString(nameof(SERVER_DATA_FORMAT));
    #else
    HttpClient httpClient(*gsmClient, SERVER_FQDN, SERVER_PORT);
    String serverData = SERVER_DATA_FORMAT;
    #endif

    stdSerial.print("Performing network request... ");

    #ifdef ENABLE_FLASH_SETTINGS
    // if (Settings.getBool(nameof(SERVER_SSL)))
    //     httpClient.connectionKeepAlive(); //Currently, this is needed for HTTPS.
    #endif

    httpClient.beginRequest();
    #ifdef ENABLE_FLASH_SETTINGS
    // if (Settings.isKey(nameof(SERVER_HEADER_AUTHORIZATION)) && strlen(Settings.getString(nameof(SERVER_HEADER_AUTHORIZATION)).c_str()))
        // httpClient.sendHeader("Authorization", Settings.getString(nameof(SERVER_HEADER_AUTHORIZATION)));
    #elif defined(SERVER_HEADER_AUTHORIZATION)
    if (strlen(SERVER_HEADER_AUTHORIZATION))
        httpClient.sendHeader("Authorization", SERVER_HEADER_AUTHORIZATION);
    #endif

    #ifdef ENABLE_FLASH_SETTINGS
    if (Settings.isKey(nameof(SERVER_ADDITIONAL_HEADERS)))
    {
        //Seperate by CLRF.
        std::vector<String> headers = SplitString(Settings.getString(nameof(SERVER_ADDITIONAL_HEADERS)), '\n');
        for (size_t i = 0; i < headers.size(); i++)
        {
            std::vector<String> header = SplitString(headers[i], ':');
            if (header.size() != 2)
                continue;
            httpClient.sendHeader(header[0], header[1]);
        }
    }
    #elif defined(SERVER_ADDITIONAL_HEADERS)
    //Seperate by CLRF.
    std::vector<String> headers = SplitString(SERVER_ADDITIONAL_HEADERS, '\n');
    for (size_t i = 0; i < headers.size(); i++)
    {
        std::vector<String> header = SplitString(headers[i], ':');
        if (header.size() != 2)
            continue;
        httpClient.sendHeader(header[0], header[1]);
    }
    #endif
    
    //Replace placeholders.
    serverData.replace("{{longitude}}", String(gps.location.lng(), 6));
    serverData.replace("{{latitude}}", String(gps.location.lat(), 6));
    serverData.replace("{{altitude}}", String(gps.altitude.meters(), 2));
    serverData.replace("{{accuracy}}", String(gps.hdop.hdop(), 2));
    //TODO: Apparently I can get the battery state of charge with "AT+CBC"?

    int err;
    #ifdef ENABLE_FLASH_SETTINGS
    switch (Settings.getUChar(nameof(SERVER_METHOD)))
    #else
    switch (SERVER_METHOD)
    #endif
    {
    case 1:
        #ifdef ENABLE_FLASH_SETTINGS
        err = httpClient.post(Settings.getString(nameof(SERVER_PATH)), Settings.getString(nameof(SERVER_HEADER_CONTENT_TYPE)), serverData);
        #else
        err = httpClient.post(SERVER_PATH, SERVER_HEADER_CONTENT_TYPE, serverData);
        #endif
        break;
    default:
        stdSerial.println("Server method not implemented.");
        return false;
    }
    httpClient.endRequest();
    if (err != 0
        || httpClient.responseStatusCode() < 200
        || httpClient.responseStatusCode() >= 300)
    {
        stdSerial.println(" | FAILED");
        return false;
    }

    stdSerial.println(" | " + String(httpClient.responseStatusCode()) + " SUCCESS");

    return true;
}

void Sleep() _ATTRIBUTE ((__noreturn__));
void Sleep()
{
    gsmClient->stop();
    modem.gprsDisconnect();

    stdSerial.println("Entering standby...");

    #if defined(ESP32)
    esp_deep_sleep_start();
    #elif defined(ESP8266)
    uint64_t sleepDuration = UPDATE_INTERVAL;
    #ifdef BATTERY_DISABLED_PIN
    sleepDuration = digitalRead(BATTERY_DISABLED_PIN) == LOW ? BATTERY_UPDATE_INTERVAL : UPDATE_INTERVAL;
    #endif
    ESP.deepSleep(sleepDuration * 1000);
    #endif
}

bool RetryWrapper(bool (*method)(void))
{
    #ifdef ENABLE_FLASH_SETTINGS
    for (size_t i = 0; i < Settings.getUShort(nameof(MAX_RETRIES)); i++)
    #else
    for (size_t i = 0; i < MAX_RETRIES; i++)
    #endif
    {
        if (method())
            return true;
        #ifdef ENABLE_FLASH_SETTINGS
        Delay(Settings.getULong(NAMEOF(RETRY_INTERVAL)) * 1000);
        #else
        Delay(RETRY_INTERVAL * 1000);
        #endif
    }

    stdSerial.println("Failed after maximum number of allowed retries");
    return false;
}

void Main()
{
    /* esp_sleep_get_wakeup_cause currently not used.
     * Originally it was in place to run specific actions on boot.
     * However for redundancy, I will run all actions on wake (i.e. process SMS, relay location, etc).
     */
    // esp_sleep_source_t wakeReason = esp_sleep_get_wakeup_cause();

    InitIO();
    ConfigureSleep();
    if (!RetryWrapper(InitModem))
        goto sleep;
    InitGps();
    if (!RetryWrapper(GetGps))
        goto sleep;
    if (!RetryWrapper(PublishData))
        goto sleep;
sleep:
    Sleep();
}

#ifdef ARDUINO
void setup()
{
    Main();
}

void loop()
{
    //Shouldn't ever be reached.
    #if defined(ESP32)
    vTaskDelete(NULL);
    #endif
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
