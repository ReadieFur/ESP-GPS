#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include "Battery.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"

void setup()
{
    SerialMon.begin(115200);
    Battery::Init();
    GPS::Init();
    GSM::Init();
    MQTT::Init();
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
    GSM::Loop();
    MQTT::Loop();

    delay(1);
}
