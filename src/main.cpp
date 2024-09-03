#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include "Config.h"
#include <SoftwareSerial.h>
#include "Scheduler.hpp"
#include "GPS.hpp"

SoftwareSerial modemSerial(MODEM_TX, MODEM_RX);
GPS gps(GPS_TX, GPS_RX);

void SerialTask(void* args)
{
    if (Serial.available())
    {
        char c = Serial.read();
        //Serial.print(c);
        modemSerial.print(c);
    }
    if (modemSerial.available())
    {
        char c = modemSerial.read();
        Serial.print(c);
    }
}

void Main()
{
    Serial.begin(9600);
    modemSerial.begin(9600);

    Scheduler::Add(1, &SerialTask);
}

#ifdef ARDUINO
void setup()
{
    Main();
}

void loop()
{
#if defined(ESP32)
    // vPortYield();
    vTaskDelete(NULL);
#elif defined(ESP8266)
    yield();
#endif
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
