#pragma once

#include "Board.h"
#include "Config.h"
#include "Service/AService.hpp"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "Logging.hpp"
#include "Helpers.h"

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
                esp_log_level_t tagLogLevel = esp_log_level_get(nameof(GPS));
                while (Serial1.available())
                {
                    char c = Serial1.read();
                    if (tagLogLevel >= esp_log_level_t::ESP_LOG_VERBOSE)
                        WRITE((const char*)&c);
                    _tinyGps.encode(c);
                }

                //Going based off of the NEO-6M which has a frequency of 5Hz.
                //I should probably scan faster than this however not much data is output so the Rx buffer shouldn't get full.
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
