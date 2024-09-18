#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include "Battery.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"
#include "Publish.hpp"

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
    // while (SerialMon.available())
    // {
    //     int c = SerialMon.read();
    //     SerialMon.write(c); //Relay the character back to the terminal (required so you can see what you are typing).
    //     SerialAT.write(c);
    // }
    // while (SerialAT.available())
    //     SerialMon.write(SerialAT.read());
    #pragma endregion

    Battery::Loop();
    GPS::Loop();
    if (!GSM::Loop())
    {
        delay(500); //TODO: Set this to the update interval.
        return;
    }
    if (!MQTT::Loop())
    {
        delay(500); //TODO: Set this to the update interval.
        return;
    }
    Publish::Loop();

    delay(500); //TODO: Set this to the update interval.
}
