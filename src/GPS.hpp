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
    size_t _bufferSize;
    char* _serialBuffer;
    uint _bufferIndex = 0;
    ulong _bufferFlushInterval;
    ulong _lastProcessTime = 0;

    static void ReadSerial(void* args)
    {
        GPS* self = static_cast<GPS*>(args);

        while (self->_gpsSerial->available())
        {
            if (self->_bufferIndex >= self->_bufferSize)
                break;

            char c = self->_gpsSerial->read();
            self->_serialBuffer[self->_bufferIndex++] = c;
            // Serial.write(c); //TODO: For testing only.
        }

        //Possibly move this to the task scheduler instead.
        ulong now = millis();
        if (self->_bufferIndex >= self->_bufferSize || now - self->_lastProcessTime > 1000)
        {
            ProcessBuffer(args);
            self->_lastProcessTime = now;
        }
    }

    static void ProcessBuffer(void* args)
    {
        GPS* self = static_cast<GPS*>(args);

        bool updated = false;
        for (size_t i = 0; i < self->_bufferIndex; i++)
            updated |= self->TinyGps.encode(self->_serialBuffer[i]);
        //No need to empty the buffer, memory is reserved so we'd just be taking up extra unnecessary cpu cycles.
        self->_bufferIndex = 0;

    }
    
public:
    TinyGPSPlus TinyGps;

    GPS(uint8_t rxPin, uint8_t txPin, ulong bufferFlushInterval = 1000)
    //From my tests, a buffer size of 512 is adequate for 1 second of data (+ some extra). So use this as a base and scale according to the buffer flush interval.
    : _bufferSize(512 * (bufferFlushInterval / 1000)), _bufferFlushInterval(bufferFlushInterval)
    {
        _gpsSerial = new SoftwareSerial(rxPin, txPin);
        _gpsSerial->begin(9600);
        
        _serialBuffer = (char*)malloc(_bufferSize * sizeof(char));
        if (_serialBuffer == nullptr)
        {
            Serial.println("Not enough memory to allocate GPS serial buffer");
            abort();
        }

        //Setting this interval to be too slow causes data to be missed or corrupted and the sentences never get complete.
        //Debating wether this is better than just updating in realtime. Less CPU is used per unit of time like this so it should theoretically be more energy efficient.
        _updateTaskId = Scheduler::Add(1, GPS::ReadSerial, this);
    }

    ~GPS()
    {
        Scheduler::Remove(_updateTaskId);

        delete _serialBuffer;

        _gpsSerial->end();
        delete _gpsSerial;
    }
};
