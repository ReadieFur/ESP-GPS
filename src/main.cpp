#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include "SerialMonitor.hpp"
#include "Motion.hpp"
#include "Battery.hpp"
#include "OTA.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"
#include "Publish.hpp"

short retryAttempts = 0;

void setup()
{
    SerialMonitor::Init();
    Motion::Init();
    Battery::Init();
    OTA::Init();
    GPS::Init();
    GSM::Init();
    MQTT::Init();
    Publish::Init();
    //TODO: Motion detector wakeup.
}

void loop()
{
    SerialMonitor::Loop();
    Motion::Loop();
    Battery::Loop(); //TODO: Possibly move this after the publish so we can send one update anyway?
    OTA::Loop();
    GPS::Loop();

    //If we fail upon first boot, retry a few times.
    bool connectFailed = true;
    do
    {
        if (!GSM::Loop() || !MQTT::Loop())
        {
            delay(100);
            retryAttempts++;
            continue;
        }

        connectFailed = false;
    }
    while (retryAttempts != -1 && connectFailed && retryAttempts < 10);
    retryAttempts = -1;
    if (connectFailed)
    {
        int sleepDuration = Battery::GetSleepDuration() / 2;
        SerialMon.printf("Fail, sleeping for: %ims\n", sleepDuration);
        Battery::LightSleep(sleepDuration);
        return;
    }
    
    if (!Publish::Loop())
    {
        int sleepDuration = Battery::GetSleepDuration() / 2;
        SerialMon.printf("Fail, sleeping for: %ims\n", sleepDuration);
        Battery::LightSleep(sleepDuration);
        return;
    }

    SerialMon.printf("Success, sleeping for: %ims\n", Battery::GetSleepDuration());
    Battery::LightSleep(Battery::GetSleepDuration());
}
