#pragma once

#include <ctime>
#include <freertos/portmacro.h>

namespace ReadieFur::EspGps
{
    enum ELocationSource
    {
        LC_Invalid = 0,
        LC_GPS = 1,
        LC_GSM = 2
    };

    struct SLocation
    {
        TickType_t age = 0;
        time_t timestamp = 0;
        double latitude = 0, longitude = 0, accuracy = 0;
    };
};
