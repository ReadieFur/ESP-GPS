#pragma once

#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include <TinyGPS++.h>

class GPS
{
private:
    static void DisplayInfo()
    {
        SerialMon.print("Location: ");
        if (Gps.location.isValid())
        {
            SerialMon.print(Gps.location.lat(), 6);
            SerialMon.print(",");
            SerialMon.print(Gps.location.lng(), 6);
        }
        else
        {
            SerialMon.print("INVALID");
        }

        SerialMon.print("  Date/Time: ");
        if (Gps.date.isValid())
        {
            SerialMon.print(Gps.date.month());
            SerialMon.print("/");
            SerialMon.print(Gps.date.day());
            SerialMon.print("/");
            SerialMon.print(Gps.date.year());
        }
        else
        {
            SerialMon.print("INVALID");
        }

        SerialMon.print(" ");
        if (Gps.time.isValid())
        {
            if (Gps.time.hour() < 10)
                SerialMon.print("0");
            SerialMon.print(Gps.time.hour());

            SerialMon.print(":");
            if (Gps.time.minute() < 10)
                SerialMon.print("0");
            SerialMon.print(Gps.time.minute());
            
            SerialMon.print(":");
            if (Gps.time.second() < 10)
                SerialMon.print("0");
            SerialMon.print(Gps.time.second());
            
            SerialMon.print(".");
            if (Gps.time.centisecond() < 10)
                SerialMon.print("0");
            SerialMon.print(Gps.time.centisecond());
        }
        else
        {
            SerialMon.print("INVALID");
        }

        SerialMon.println();
    }

public:
    static TinyGPSPlus Gps;

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
            bool encodeResult = Gps.encode(c);
            if (encodeResult)
            {
                #ifdef DUMP_GPS_DATA
                DisplayInfo();
                #endif
            }
        }
    }
};

TinyGPSPlus GPS::Gps;
