#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include "SerialMonitor.hpp"
// #include "Motion.hpp"
#include "Battery.hpp"
#ifdef ENABLE_AP
#include "OTA.hpp"
#endif
#include "GPS.hpp"
#include "Location.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"
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

    #ifdef MOTION_MODULE
    Motion::Init();
    #endif
    Battery::Init();
    #ifdef ENABLE_AP
    OTA::Init();
    #endif
    GPS::Init();
    GSM::Init();
    Location::Init();
    MQTT::Init();
    Publish::Init();
}

void loop()
{
    SerialMonitor::Loop();
    #ifdef MOTION_MODULE
    Motion::Loop();
    #endif
    Battery::Loop(); //TODO: Possibly move this after the publish so we can send one update anyway?
    #ifdef ENABLE_AP
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
        SerialMon.printf("Fail, sleeping for: %ims\n", sleepDuration);
        Battery::LightSleep(sleepDuration);
        return;
    }

    SerialMon.printf("Success, sleeping for: %ims\n", Battery::GetSleepDuration());
    Battery::LightSleep(Battery::GetSleepDuration());
}
