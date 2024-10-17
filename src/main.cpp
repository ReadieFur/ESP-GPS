#include <Arduino.h>
#include "Board.h"
#include "Config.h"

#if defined(MPU_INT) && defined(MPU_SDA) && defined(MPU_SCL)
#define MOTION_MODULE
#endif

#include "SerialMonitor.hpp"
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

short retryAttempts = 0;

void setup()
{
    SerialMonitor::Init();

    //TODO: Check if wakeup source was from motion.
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
        case ESP_SLEEP_WAKEUP_EXT0: SerialMon.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1: SerialMon.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER: SerialMon.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: SerialMon.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP: SerialMon.println("Wakeup caused by ULP program"); break;
        default: SerialMon.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }

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
        int sleepDuration = Battery::GetSleepDuration() / 2;
        SerialMon.printf("Fail, deep sleeping for: %ims\n", sleepDuration);
        //Deep sleep will cause the program to restart, which we want if this fails.
        Battery::DeepSleep(sleepDuration);
        return;
    }
    Location::Init();
    MQTT::Init();
    Publish::Init();
}

void loop()
{
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
        int sleepDuration = Battery::GetSleepDuration() / 2;
        SerialMon.printf("Fail, deep sleeping for: %ims\n", sleepDuration);
        //Only light sleep if we had a successful run.
        //TODO: Change this so that we only enter deep sleep of the modem stops responding as we can also fail here if there is a poor connection, which is not critical.
        Battery::DeepSleep(sleepDuration);
        return;
    }

    SerialMon.printf("Success, light sleeping for: %ims\n", Battery::GetSleepDuration());
    Battery::LightSleep(Battery::GetSleepDuration());
}
