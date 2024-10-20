#include <Arduino.h>
#include "Board.h"
#include "Config.h"

#if defined(MPU_INT) && defined(MPU_SDA) && defined(MPU_SCL)
#define MOTION_MODULE
#endif

#include "SerialMonitor.hpp"
#include "Storage.hpp"
#ifdef MOTION_MODULE
#include "Motion.hpp"
#endif
#include "Battery.hpp"
#ifdef AP_SSID
#include "OTA.hpp"
#endif
#include "GPS.hpp"
#include "Location.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"
#include "API.hpp"
#include "Publish.hpp"
#include <WString.h>
#include <vector>

short retryAttempts = 0;

String MsToDurationString(uint durationMs)
{
    //Calculate each time component.
    unsigned long totalMilliseconds = durationMs % 1000;
    unsigned long totalSeconds = durationMs / 1000;
    unsigned long seconds = totalSeconds % 60;
    unsigned long totalMinutes = totalSeconds / 60;
    unsigned long minutes = totalMinutes % 60;
    unsigned long totalHours = totalMinutes / 60;
    unsigned long hours = totalHours % 24;
    unsigned long days = totalHours / 24;
    
    std::vector<String> parts;
    if (days > 0)
        parts.push_back(String(days) + " day" + (days > 1 ? "s" : ""));
    if (hours > 0)
        parts.push_back(String(hours) + " hour" + (hours > 1 ? "s" : ""));
    if (minutes > 0)
        parts.push_back(String(minutes) + " minute" + (minutes > 1 ? "s" : ""));
    if (seconds > 0)
        parts.push_back(String(seconds) + " second" + (seconds > 1 ? "s" : ""));
    if (totalMilliseconds > 0 || parts.size() == 0)
        parts.push_back(String(totalMilliseconds) + " millisecond" + (totalMilliseconds != 1 ? "s" : ""));

    String durationString = "";
    for (size_t i = 0; i < parts.size(); i++)
    {
        String part = parts.at(i);
        
        if (i == 0)
            durationString += part;
        else if (i == parts.size() - 1)
            durationString += " and " + part;
        else
            durationString += " " + part;
    }

    return durationString;
}

void DeviceSleep(uint durationMs, bool deepSleep)
{
    String sleepDurationString = MsToDurationString(durationMs);
    SerialMon.printf("%s sleeping for %s.\n", deepSleep ? "Deep" : "Light", sleepDurationString);

    if (deepSleep)
        Battery::DeepSleep(durationMs);
    else
        Battery::LightSleep(durationMs);
}

void LogWakeReason()
{
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
        case ESP_SLEEP_WAKEUP_EXT0: SerialMon.println("Wakeup caused by external signal using RTC_IO."); break;
        case ESP_SLEEP_WAKEUP_EXT1: SerialMon.println("Wakeup caused by external signal using RTC_CNTL."); break;
        case ESP_SLEEP_WAKEUP_TIMER: SerialMon.println("Wakeup caused by timer."); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: SerialMon.println("Wakeup caused by touchpad."); break;
        case ESP_SLEEP_WAKEUP_ULP: SerialMon.println("Wakeup caused by ULP program."); break;
        default: SerialMon.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }
}

void setup()
{
    SerialMonitor::Init();
    LogWakeReason();
    Storage::Init();
    API::Init();
    #ifdef MOTION_MODULE
    Motion::Init();
    #endif
    Battery::Init();
    #ifdef AP_SSID
    OTA::Init();
    #endif
    GPS::Init();
    if (!GSM::Init())
    {
        SerialMon.print("Fail, ");
        //Deep sleep will cause the program to restart, which we want if this fails.
        DeviceSleep(Battery::GetSleepDuration() / 2, true);
        return; //Not reached.
    }
    Location::Init();
    MQTT::Init();
    Publish::Init();
}

void loop()
{
    LogWakeReason();
    SerialMonitor::Loop();
    API::Loop();
    #ifdef MOTION_MODULE
    Motion::Loop();
    #endif
    Battery::Loop(); //TODO: Possibly move this after the publish so we can send one update anyway?
    #ifdef AP_SSID
    OTA::Loop();
    #endif
    // GPS::Loop();
    Location::Loop(); //TODO: Rerun this if it fails.

    //If we fail to send data, retry a few times.
    bool failed = true;
    do
    {
        if (!GSM::Loop() || !MQTT::Loop() || !Publish::Loop())
        {
            delay(100);
            retryAttempts++;
            continue;
        }

        failed = false;
    }
    while (failed && retryAttempts < 10);
    if (failed)
    {
        SerialMon.print("Fail, ");
        //Only light sleep if we had a successful run.
        //TODO: Change this so that we only enter deep sleep of the modem stops responding as we can also fail here if there is a poor connection, which is not critical.
        DeviceSleep(Battery::GetSleepDuration() / 2, true);
        return;
    }

    SerialMon.print("Success, ");
    //TODO: Deep sleep the device here but keep the GPS alive to maintain a fix.
    DeviceSleep(Battery::GetSleepDuration(), false);
}
