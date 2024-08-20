//https://randomnerdtutorials.com/esp32-sim800l-publish-data-to-cloud/
//https://randomnerdtutorials.com/guide-to-neo-6m-gps-module-with-arduino/
//https://github.com/vshymanskyy/TinyGSM/blob/master/examples/HttpsClient/HttpsClient.ino
//https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/

#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>
#include "Config.h"
#include <Preferences.h>
#include <stdlib.h>
#include <esp32-hal-gpio.h>
#include <SoftwareSerial.h>
#include <TinyGsmClient.h>
#include <TinyGPSPlus.h>
#include <HttpClient.h>
#include <chrono>
#include <vector>
#ifdef _DEBUG
  #include <StreamDebugger.h>
#endif

#ifdef BATTERY_DISABLED_PIN
#ifndef BATTERY_UPDATE_INTERVAL
#error "Please define the update interval for when on battery power."
#endif
#endif
#if SERVER_METHOD < 1 || SERVER_METHOD > 1
#error "Server method not implemented."
#endif

#define NAMEOF(name) #name
#define StdSerial Serial
#define ModemSerial Serial1

Preferences Settings;

#ifdef _DEBUG
StreamDebugger _modemDebugger(ModemSerial, StdSerial);
TinyGsm Modem(_modemDebugger);
#else
TinyGsm Modem(ModemSerial);
#endif
TinyGsmClient* GsmClient = nullptr;

SoftwareSerial GpsSerial(GPS_RX, GPS_TX);
#ifdef _DEBUG
StreamDebugger _gpsDebugger(GpsSerial, StdSerial);
#endif
TinyGPSPlus Gps;

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

void InitPreferences()
{
    Settings.begin("config", false);

    #ifdef RESTORE_DEFAULTS_ON_FLASH
    Settings.clear();
    #endif

    if (!Settings.isKey(NAMEOF(UPDATE_INTERVAL)))
        Settings.putULong(NAMEOF(UPDATE_INTERVAL), UPDATE_INTERVAL);
    if (!Settings.isKey(NAMEOF(BATTERY_UPDATE_INTERVAL)))
        Settings.putULong(NAMEOF(BATTERY_UPDATE_INTERVAL), BATTERY_UPDATE_INTERVAL);
    if (!Settings.isKey(NAMEOF(MAX_RETRIES)))
        Settings.putUShort(NAMEOF(MAX_RETRIES), MAX_RETRIES);
    if (!Settings.isKey(NAMEOF(RETRY_INTERVAL)))
        Settings.putULong(NAMEOF(RETRY_INTERVAL), RETRY_INTERVAL);
    
    if (!Settings.isKey(NAMEOF(RESPOND_TO_API_CALLS)))
        Settings.putBool(NAMEOF(RESPOND_TO_API_CALLS), RESPOND_TO_API_CALLS);

    if (!Settings.isKey(NAMEOF(SERVER_FQDN)))
        Settings.putString(NAMEOF(SERVER_FQDN), SERVER_FQDN);
    if (!Settings.isKey(NAMEOF(SERVER_PORT)))
        Settings.putUShort(NAMEOF(SERVER_PORT), SERVER_PORT);
    if (!Settings.isKey(NAMEOF(SERVER_PATH)))
        Settings.putString(NAMEOF(SERVER_PATH), SERVER_PATH);
    if (!Settings.isKey(NAMEOF(SERVER_SSL)))
        Settings.putBool(NAMEOF(SERVER_SSL), SERVER_SSL);
    if (!Settings.isKey(NAMEOF(SERVER_METHOD)))
        Settings.putUChar(NAMEOF(SERVER_METHOD), SERVER_METHOD);
    if (!Settings.isKey(NAMEOF(SERVER_HEADER_CONTENT_TYPE)))
        Settings.putString(NAMEOF(SERVER_HEADER_CONTENT_TYPE), SERVER_HEADER_CONTENT_TYPE);
    if (!Settings.isKey(NAMEOF(SERVER_HEADER_AUTHORIZATION)))
        Settings.putString(NAMEOF(SERVER_HEADER_AUTHORIZATION), SERVER_HEADER_AUTHORIZATION);
    if (!Settings.isKey(NAMEOF(SERVER_DATA_FORMAT)))
        Settings.putString(NAMEOF(SERVER_DATA_FORMAT), SERVER_DATA_FORMAT);
    if (!Settings.isKey(NAMEOF(SERVER_ADDITIONAL_HEADERS)))
        Settings.putString(NAMEOF(SERVER_ADDITIONAL_HEADERS), SERVER_ADDITIONAL_HEADERS);
}

bool InitModem()
{
    //Configure additional pins manually (not controlled by external libraries).
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

    //Begin communication with the modem.
    ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    Modem.restart();

    #ifdef MODEM_PIN
    //Unlock the SIM.
    if (strlen(MODEM_PIN)
        && Modem.getSimStatus() != 3
        && !Modem.simUnlock(MODEM_PIN));
    {
        StdSerial.println("Failed to unlock SIM card.");
        abort();
    }
    #endif

    #ifdef ENABLE_SMS_API
    //Configure SMS storage and notifications.
    Modem.sendAT(GF("+CMGF=1"));
    Modem.waitResponse();
    // Modem.sendAT(GF("+CPMS=\"SM\",\"SM\",\"SM\"")); //Store on SIM card (persistent).
    Modem.sendAT(GF("+CPMS=\"ME\",\"ME\",\"ME\"")); //Store in memory (volatile).
    Modem.waitResponse();
    Modem.sendAT(GF("+CNMI=2,1,0,0,0"));
    Modem.waitResponse();
    #endif

    GsmClient = Settings.getBool(NAMEOF(SERVER_SSL))
        ? new TinyGsmClientSecure(Modem)
        : new TinyGsmClient(Modem);

    StdSerial.print("Connecting to APN: ");
    StdSerial.print(MODEM_APN);
    if (!Modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
    {
        StdSerial.println(" | FAILED");
        return false;
    }

    return true;
}

void InitGps()
{
    GpsSerial.begin(9600);
}

void ConfigureSleep()
{
    uint64_t sleepDuration = 0;
    uint64_t gpioMask = 0;

    #ifdef MODEM_RING
    pinMode(MODEM_RING, INPUT);
    if (!esp_sleep_is_valid_wakeup_gpio(MODEM_RING))
    {
        StdSerial.println(String("Invalid GPIO for ") + NAMEOF(MODEM_RING));
        abort();
    }
    gpioMask |= BIT(MODEM_RING);
    #endif

    #ifdef BATTERY_DISABLED_PIN
    pinMode(BATTERY_DISABLED_PIN, INPUT);
    if (!esp_sleep_is_valid_wakeup_gpio(BATTERY_DISABLED_PIN))
    {
        StdSerial.println(String("Invalid GPIO for ") + NAMEOF(BATTERY_DISABLED_PIN));
        abort();
    }
    gpioMask |= BIT(BATTERY_DISABLED_PIN);
    sleepDuration = Settings.getULong(digitalRead(BATTERY_DISABLED_PIN) == LOW ? NAMEOF(BATTERY_UPDATE_INTERVAL) : NAMEOF(UPDATE_INTERVAL));
    #else
    sleepDuration = Settings.getULong(NAMEOF(UPDATE_INTERVAL));
    #endif

    esp_sleep_enable_timer_wakeup(sleepDuration * 1000000);
    esp_deep_sleep_enable_gpio_wakeup(BIT(MODEM_RING), ESP_GPIO_WAKEUP_GPIO_HIGH);
}

#ifdef ENABLE_SMS_API
void ProcessSMS()
{
    //List all unread SMS messages in internal memory.
    Modem.sendAT(GF("+CMGL=\"REC UNREAD\""));
    String response = Modem.stream.readString();
    // StdSerial.println(response); //Print response for debugging.

    //Parse the response to find and read each unread SMS.
    int index = 0;
    while ((index = response.indexOf("+CMGL: ", index)) != -1)
    {
        int commaPos = response.indexOf(',', index);
        int smsIndex = response.substring(index + 7, commaPos).toInt();

        //Read the SMS message at the specified index.
        Modem.sendAT(GF("+CMGR="), smsIndex);
        String smsContent = Modem.stream.readString();
        //TODO: Seperate data fields.
        StdSerial.println(smsContent); //Print SMS content for debugging.

        String responseMessage = "Unknown SMS API call.";

        //Process message.
        if (strcasecmp(smsContent.c_str(), "publish"))
        {
            //Publish GPS data (occurs anyway so for now we do nothing here).
            //Though if RESPOND_TO_API_CALLS is true perhaps we could send the location back over SMS.
        }
        else if (smsContent.indexOf('=') != -1)
        {            
            int seperatorIndex = smsContent.indexOf('=');
            //Test if message is a configuration update.
            String key = smsContent.substring(0, seperatorIndex);
            key.toUpperCase();
            String value = smsContent.substring(seperatorIndex + 1);

            //All valid settings are predefined within the settings so if the key dosent exist than an invalid key has been provided.
            if (Settings.isKey(key.c_str()))
            {
                //TODO: Store as the correct datatype.
                Settings.putString(key.c_str(), value);
            }
            else
            {
                StdSerial.println("Invalid settings key provided in SMS API call: " + key);
            }
        }

        //Delete the SMS message after reading.
        Modem.sendAT(GF("+CMGD="), smsIndex);
        Modem.waitResponse();

        if (Settings.getBool(NAMEOF(RESPOND_TO_API_CALLS)))
        {
            // Modem.sendSMS(, responseMessage);
        }

        //Move to the next SMS in the response.
        index = commaPos;
    }
}
#endif

//TODO: Possibly add ability to accept incoming calls to listen into (as a security feature, not a stalking feature).

bool GetGps()
{
    StdSerial.print("Obtaining GPS data...");
    ulong start = millis();
    while (!Gps.location.isUpdated() && GpsSerial.available() > 0 && millis() - start < std::chrono::seconds(2).count())
        Gps.encode(GpsSerial.read());

    if (!Gps.location.isUpdated())
    {
        StdSerial.println(" | FAILED");
        return false;
    }

    return true;
}

bool PublishData()
{
    StdSerial.print("Waiting for network...");
    if (!Modem.waitForNetwork())
    {
        StdSerial.println(" | FAILED");
        return false;
    }

    HttpClient httpClient(*GsmClient,
        Settings.getString(NAMEOF(SERVER_FQDN)),
        Settings.getUShort(NAMEOF(SERVER_PORT)));
    String serverData = Settings.getString(NAMEOF(SERVER_DATA_FORMAT));

    StdSerial.print("Performing network request... ");

    if (Settings.getBool(NAMEOF(SERVER_SSL)))
        httpClient.connectionKeepAlive(); //Currently, this is needed for HTTPS.

    httpClient.beginRequest();
    if (Settings.isKey(NAMEOF(SERVER_HEADER_AUTHORIZATION)) && strlen(Settings.getString(NAMEOF(SERVER_HEADER_AUTHORIZATION)).c_str()))
        httpClient.sendHeader("Authorization", Settings.getString(NAMEOF(SERVER_HEADER_AUTHORIZATION)));

    if (Settings.isKey(NAMEOF(SERVER_ADDITIONAL_HEADERS)))
    {
        //Seperate by CLRF.
        std::vector<String> headers = SplitString(Settings.getString(NAMEOF(SERVER_ADDITIONAL_HEADERS)), '\n');
        for (size_t i = 0; i < headers.size(); i++)
        {
            std::vector<String> header = SplitString(headers[i], ':');
            if (header.size() != 2)
                continue;
            httpClient.sendHeader(header[0], header[1]);
        }
    }
    
    //Replace placeholders.
    serverData.replace("{{longitude}}", String(Gps.location.lng(), 6));
    serverData.replace("{{latitude}}", String(Gps.location.lat(), 6));
    serverData.replace("{{altitude}}", String(Gps.altitude.meters(), 2));
    serverData.replace("{{accuracy}}", String(Gps.hdop.hdop(), 2));
    //TODO: Apparently I can get the battery state of charge with "AT+CBC"?

    int err;
    switch (Settings.getUChar(NAMEOF(SERVER_METHOD)))
    {
    case 1:
        err = httpClient.post(Settings.getString(NAMEOF(SERVER_PATH)), Settings.getString(NAMEOF(SERVER_HEADER_CONTENT_TYPE)), serverData);
        break;
    default:
        StdSerial.println("Server method not implemented.");
        return false;
    }
    httpClient.endRequest();
    if (err != 0
        || httpClient.responseStatusCode() < 200
        || httpClient.responseStatusCode() >= 300)
    {
        StdSerial.println(" | FAILED");
        return false;
    }

    StdSerial.println(" | " + String(httpClient.responseStatusCode()) + " SUCCESS");

    return true;
}

void Sleep() _ATTRIBUTE ((__noreturn__));
void Sleep()
{
    GsmClient->stop();
    Modem.gprsDisconnect();

    //TODO: Put GPS to sleep.

    StdSerial.println("Entering standby...");
    esp_deep_sleep_start();
}

bool RetryWrapper(bool (*method)(void))
{
    for (size_t i = 0; i < Settings.getUShort(NAMEOF(MAX_RETRIES)); i++)
    {
        if (method())
            return true;
        vTaskDelay(pdMS_TO_TICKS(Settings.getULong(NAMEOF(RETRY_INTERVAL)) * 1000));
    }

    StdSerial.println("Failed after maximum number of allowed retries");
    return false;
}

void Main()
{
    /* esp_sleep_get_wakeup_cause currently not used.
     * Originally it was in place to run specific actions on boot.
     * However for redundancy, I will run all actions on wake (i.e. process SMS, relay location, etc).
     */
    // esp_sleep_source_t wakeReason = esp_sleep_get_wakeup_cause();

    InitPreferences();

    ConfigureSleep();

    if (!RetryWrapper(InitModem))
        goto sleep;
    #ifdef ENABLE_SMS_API
    ProcessSMS();
    #endif

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
    vTaskDelete(NULL);
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
