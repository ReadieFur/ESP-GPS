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
        void SetupGPIO()
        {
            #ifdef GPS_PPS
            pinMode(GPS_PPS, INPUT_PULLDOWN);
            #endif
            #ifdef GPS_WAKEUP
            pinMode(GPS_PPS, INPUT_PULLDOWN);
            #endif
            pinMode(GPS_RX, INPUT_PULLDOWN);
            pinMode(GPS_TX, OUTPUT);
        }

        void PowerOn()
        {
            Serial1.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
            #ifdef GPS_WAKEUP
            gpio_hold_dis((gpio_num_t)GPS_WAKEUP);
            digitalWrite(GPS_WAKEUP, !GPS_SLEEP_LEVEL);
            #endif
        }

        void PowerOff()
        {
            Serial1.end();
            #ifdef GPS_WAKEUP
            digitalWrite(GPS_WAKEUP, GPS_SLEEP_LEVEL);
            gpio_hold_en((gpio_num_t)GPS_WAKEUP);
            #endif
        }

        void LogLocation()
        {
            if (TinyGps.location.isValid())
            {
                LOGI(nameof(Location), "Location: %f, %f", TinyGps.location.lat(), TinyGps.location.lng());
            }
        }

    protected:
        void RunServiceImpl() override
        {
            PowerOn();

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                bool logVerbose = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE;
                while (Serial1.available())
                {
                    char c = Serial1.read();
                    #if true
                    if (logVerbose)
                        WRITE(c);
                    #endif
                    bool encodeResult = TinyGps.encode(c);
                    if (encodeResult && logVerbose)
                        LogLocation();
                }

                //Going based off of the NEO-6M which has a frequency of 5Hz.
                //I should probably scan faster than this however not much data is output so the Rx buffer shouldn't get full.
                vTaskDelay(pdMS_TO_TICKS(1000 / 5));
            }

            PowerOff();
        }

    public:
        TinyGPSPlus TinyGps;

        GPS()
        {
            ServiceEntrypointStackDepth += 1024;
            SetupGPIO();
        }

        ~GPS()
        {
            PowerOff();
        }
    };
};
