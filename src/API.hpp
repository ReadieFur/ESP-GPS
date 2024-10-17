#pragma once

#include <WString.h>
#include "Board.h"

class API
{
private:
public:
    static void Init() {}

    static void Loop() {}

    static void ProcessMessage(String message)
    {
        message.trim();
        if (message == "ping")
            SerialMon.println("pong");
    }
};
