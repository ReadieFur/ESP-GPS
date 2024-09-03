#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include "Config.h"
#include <SoftwareSerial.h>
#include "Scheduler.hpp"
#include "GSM.hpp"
#include "GPS.hpp"

GSM gsm(MODEM_TX, MODEM_RX);
GPS gps(GPS_TX, GPS_RX);

void GpsTask(void* args)
{
    if (!gps.TinyGps.location.isUpdated())
        return;

    Serial.println();
    Serial.print("Lat: ");
    Serial.println(gps.TinyGps.location.lat(), 6);
    Serial.print("Lng: ");
    Serial.println(gps.TinyGps.location.lng(), 6);
}

void Main()
{
    Serial.begin(9600);

    Scheduler::Add(1000, &GpsTask);
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
