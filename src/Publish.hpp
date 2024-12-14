#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include "Service/AService.hpp"
#include "Location.hpp"
#include "MQTT.hpp"
#include <ArduinoJson.h>
#include <WString.h>
#include "Logging.hpp"
#include "Board.h"
#ifdef BATTERY_ADC
#include "Battery.hpp"
#endif
#include <esp_sleep.h>
#include "Storage.hpp"
#include "Checkpoint.hpp"

namespace ReadieFur::EspGps
{
    //TODO: The stack allocated here is more than enough, I should reduce it in the future.
    class Publish : public Service::AService
    {
    private:
        Location* _locationService = nullptr;
        MQTT* _mqttService = nullptr;
        #ifdef BATTERY_ADC
        Battery* _batteryService = nullptr;
        #endif
        JsonDocument _jsonBuffer;
        String _stringBuffer;

        //I am keeping the buffers in the heap to reduce the amount of stack space I need to allocate.
        void ClearBuffers()
        {
            _jsonBuffer.clear();
            _stringBuffer.clear();
        }

    protected:
        void RunServiceImpl() override
        {
            _locationService = GetService<Location>();
            _mqttService = GetService<MQTT>();
            #ifdef BATTERY_ADC
            _batteryService = GetService<Battery>();
            #endif

            _mqttService->WaitForConnection();


            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                ClearBuffers();

                Location::SLocation location;
                _locationService->GetLocation(location);
                if (location.type == Location::ELocationType::Invalid)
                {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    continue;
                }

                _jsonBuffer["trigger"] = GetConfig(int, trigger);

                _jsonBuffer["type"] = location.type;
                _jsonBuffer["time"] = location.timestamp;
                _jsonBuffer["lat"] = location.latitude;
                _jsonBuffer["lng"] = location.longitude;
                _jsonBuffer["acc"] = location.accuracy;

                #ifdef BATTERY_ADC
                double batteryVoltage, chargeVoltage;
                Battery::EState batteryState;
                _batteryService->GetStatus(&batteryVoltage, &chargeVoltage, &batteryState);
                _jsonBuffer["bat"] = batteryVoltage;
                _jsonBuffer["bat_state"] = batteryState;
                #if defined(CHARGE_ADC) && false
                _jsonBuffer["chg"] = chargeVoltage;
                #endif
                #endif

                if (!_mqttService->WaitForConnection(pdMS_TO_TICKS(1000)))
                {
                    //Don't wait because the wait will have been performed above if the expression evaluates to false.
                    continue;
                }

                serializeJson(_jsonBuffer, _stringBuffer);

                if (!_mqttService->Publish(_stringBuffer.c_str(), configIDLE_TASK_STACK_SIZE + 2048, pdTICKS_TO_MS(1000)))
                {
                    LOGE(nameof(Publish), "Failed to publish MQTT message.");
                }
                else
                {
                    LOGV(nameof(Publish), "Successfully published MQTT message.");
                }

                //TODO: Change these intervals to be dynamic.
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            _mqttService = nullptr;
            _locationService = nullptr;
            #ifdef BATTERY_ADC
            _batteryService = nullptr;
            #endif
            ClearBuffers();
        }

    public:
        Publish()
        {
            ServiceEntrypointStackDepth += 2048;
            AddDependencyType<Location>();
            AddDependencyType<MQTT>();
            #ifdef BATTERY_ADC
            AddDependencyType<Battery>();
            #endif
        }
    };
};
