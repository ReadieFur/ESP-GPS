#pragma once

#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include <TinyGPS++.h>

class GPS
{
private:
    static TinyGPSPlus gps;

    static void DisplayInfo()
    {
        SerialMon.print("Location: ");
        if (gps.location.isValid())
        {
            SerialMon.print(gps.location.lat(), 6);
            SerialMon.print(",");
            SerialMon.print(gps.location.lng(), 6);
        }
        else
        {
            SerialMon.print("INVALID");
        }

        SerialMon.print("  Date/Time: ");
        if (gps.date.isValid())
        {
            SerialMon.print(gps.date.month());
            SerialMon.print("/");
            SerialMon.print(gps.date.day());
            SerialMon.print("/");
            SerialMon.print(gps.date.year());
        }
        else
        {
            SerialMon.print("INVALID");
        }

        SerialMon.print(" ");
        if (gps.time.isValid())
        {
            if (gps.time.hour() < 10) SerialMon.print("0");
            SerialMon.print(gps.time.hour());
            SerialMon.print(":");
            if (gps.time.minute() < 10) SerialMon.print("0");
            SerialMon.print(gps.time.minute());
            SerialMon.print(":");
            if (gps.time.second() < 10) SerialMon.print("0");
            SerialMon.print(gps.time.second());
            SerialMon.print(".");
            if (gps.time.centisecond() < 10) SerialMon.print("0");
            SerialMon.print(gps.time.centisecond());
        }
        else
        {
            SerialMon.print("INVALID");
        }

        SerialMon.println();
    }

public:
    static void Init()
    {
        SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    }

    static void Loop()
    {
        while (SerialGPS.available())
        {
            int c = SerialGPS.read();
            #ifdef DUMP_GPS_COMMANDS
            SerialMon.write(c);
            #endif
            if (gps.encode(c))
                DisplayInfo();
        }
    }
};

TinyGPSPlus GPS::gps;
