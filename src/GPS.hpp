#pragma once

#include "Board.h"
#include "Config.h"
#include "Service/AService.hpp"
#include <TinyGPS++.h>
#include <HardwareSerial.h>

namespace ReadieFur::EspGps
{
    class GPS : public Service::AService
    {
    private:
        TinyGPSPlus _tinyGps;

    protected:
        void RunServiceImpl() override
        {
            Serial1.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                while (Serial1.available())
                {
                    char c = Serial1.read();
                    _tinyGps.encode(c);
                }

                //Going based off of the NEO-6M which has a frequency of 5Hz.
                vTaskDelay(pdMS_TO_TICKS(1000 / 5));
            }

            Serial1.end();
        }

    public:
        GPS()
        {
            ServiceEntrypointStackDepth += 1024;
        }
    };
};
