#pragma once

#include "Config.h"
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "Scheduler.hpp"

class GPS
{
private:
    SoftwareSerial* _gpsSerial;
    ulong _updateTaskId;

    static void ReadSerial(void* args)
    {
        GPS* self = static_cast<GPS*>(args);

        while (self->_gpsSerial->available())
        {
            char c = self->_gpsSerial->read();
            self->TinyGps.encode(c);
            #ifdef RELAY_SERIAL
            Serial.write(c);
            #endif
        }
    }
    
public:
    TinyGPSPlus TinyGps;
    #ifdef DEBUG
    SoftwareSerial** DebugSerial = &_gpsSerial;
    #endif

    GPS(uint8_t rxPin, uint8_t txPin)
    {
        _gpsSerial = new SoftwareSerial(rxPin, txPin);
        _gpsSerial->begin(9600);
        
        //Setting this interval to be too slow causes data to be missed or corrupted and the sentences never get complete.
        //Debating wether this is better than just updating in realtime. Less CPU is used per unit of time like this so it should theoretically be more energy efficient.
        _updateTaskId = Scheduler::Add(100, GPS::ReadSerial, this);
    }

    ~GPS()
    {
        Scheduler::Remove(_updateTaskId);

        _gpsSerial->end();
        delete _gpsSerial;
    }
};
