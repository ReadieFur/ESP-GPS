#pragma once

#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include <TinyGPS++.h>

class GPS
{
public:
    static TinyGPSPlus Gps;

    static void Init()
    {
        SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    }

    static void Loop()
    {
        //Do this a couple of times so we don't miss anything.
        for (size_t i = 0; i < 3; i++)
        {
            while (SerialGPS.available())
            {
                int c = SerialGPS.read();
                #ifdef DUMP_GPS_COMMANDS
                SerialMon.write(c);
                #endif
                Gps.encode(c);
            }

            delay(50);
        }
    }
};

TinyGPSPlus GPS::Gps;
