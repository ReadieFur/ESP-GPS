#pragma once

#include <Arduino.h>
#include "Service/AService.hpp"

namespace ReadieFur::EspGps
{
    class SerialMonitor : public Service::AService
    {
    protected:
        void RunServiceImpl() override
        {
            Serial.begin(115200, SERIAL_8N1, -1, -1); //Default pinout for esp32 when -1.

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                while (Serial.available())
                {
                    char c = Serial.read();
                    Serial.write(c);
                }

                vTaskDelay(pdMS_TO_TICKS(1000 / 200));
            }

            Serial.end();
        }
    
    public:
        SerialMonitor()
        {
            //It seems to be that any task that needs to interact with STDOUT needs at least an additional 1024 bytes on the stack.
            //My guess is this is for the internal buffer when formatting the string.
            ServiceEntrypointStackDepth += 1024;
        }
    };
};
