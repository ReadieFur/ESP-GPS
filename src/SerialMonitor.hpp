#pragma once

#include "Board.h"
#include <WString.h>
#include "API.hpp"

class SerialMonitor
{
private:
    static String _buffer;

public:
    static void Init()
    {
        SerialMon.begin(115200);
        // _buffer.reserve(256); //This should be plenty, helps prevent against memory corruption when building the buffer.
    }

    static void Loop()
    {
        while (SerialMon.available())
        {
            int c = SerialMon.read();
            SerialMon.write(c); //Relay the character back to the terminal (required so you can see what you are typing).
            _buffer += (char)c;
            if (c == '\n')
            {
                API::ProcessMessage(_buffer);
                _buffer.clear();
            }

            // SerialAT.write(c);
        }
        // while (SerialAT.available())
        //     SerialMon.write(SerialAT.read());
    }
};

String SerialMonitor::_buffer;
