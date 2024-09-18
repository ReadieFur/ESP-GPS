#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include "Battery.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"
#include "Publish.hpp"

short retryAttempts = 0;

void setup()
{
    SerialMon.begin(115200);
    Battery::Init();
    GPS::Init();
    GSM::Init();
    MQTT::Init();
    Publish::Init();
}

void loop()
{
    #pragma region SerialMon
    while (SerialMon.available())
    {
        int c = SerialMon.read();
        SerialMon.write(c); //Relay the character back to the terminal (required so you can see what you are typing).
        // SerialAT.write(c);
    }
    // while (SerialAT.available())
    //     SerialMon.write(SerialAT.read());
    #pragma endregion

    Battery::Loop();
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
        Serial.printf("Fail, sleeping for: %ims\n", sleepDuration);
        Battery::LightSleep(sleepDuration);
        return;
    }
    
    if (!Publish::Loop())
    {
        int sleepDuration = Battery::GetSleepDuration() / 2;
        Serial.printf("Fail, sleeping for: %ims\n", sleepDuration);
        Battery::LightSleep(sleepDuration);
        return;
    }

    Serial.printf("Success, sleeping for: %ims\n", Battery::GetSleepDuration());
    Battery::LightSleep(Battery::GetSleepDuration());
}
