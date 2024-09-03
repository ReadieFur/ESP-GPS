#pragma once

#include "Config.h"
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "Scheduler.hpp"

class GPS
{
private:
    SoftwareSerial gpsSerial;
    TinyGPSPlus tinyGps;
    ulong updateTaskId;

    static void Update(void* args)
    {
        GPS* self = static_cast<GPS*>(args);

        bool updated = false;
        while (self->gpsSerial.available())
        {
            char c = self->gpsSerial.read();
            updated |= self->tinyGps.encode(c);
            Serial.write(c);
        }
        // if (!updated)
        //     return;

        // if (self->tinyGps.location.isValid())
        // {
            Serial.println();
            Serial.print("Lng: ");
            Serial.println(self->tinyGps.location.lng() , 6);
            Serial.print("Lat: ");
            Serial.println(self->tinyGps.location.lat() , 6);
            Serial.print("Sat: ");
            Serial.println(self->tinyGps.satellites.value(), 6);
            Serial.print("Age: ");
            Serial.println(self->tinyGps.location.age(), 6);
        // }
    }

public:
    GPS(uint8_t rxPin, uint8_t txPin, ulong updateInterval = 1000)
    {
        gpsSerial.begin(9600, SWSERIAL_8N1, rxPin, txPin);
        updateTaskId = Scheduler::Add(updateInterval, GPS::Update, this);
    }

    ~GPS()
    {
        Scheduler::Remove(updateTaskId);
    }
};
