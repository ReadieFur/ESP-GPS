#pragma once

#include "Board.h"

class SerialMonitor
{
public:
    static void Init()
    {
        SerialMon.begin(115200);
    }

    static void Loop()
    {
        while (SerialMon.available())
        {
            int c = SerialMon.read();
            SerialMon.write(c); //Relay the character back to the terminal (required so you can see what you are typing).
            // SerialAT.write(c);
        }
        // while (SerialAT.available())
        //     SerialMon.write(SerialAT.read());
    }
};
