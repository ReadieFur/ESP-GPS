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
#include "SLocation.h"
#include <Event/ManualResetEvent.hpp>
#include "GPS.hpp"
#include <Event/Waitable.hpp>
#include <vector>
#include <Event/AWaitHandle.hpp>

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
        Event::ManualResetEvent _wakeEvent;
        #endif
        JsonDocument _jsonBuffer;
        String _stringBuffer;

        //I am keeping the buffers in the heap to reduce the amount of stack space I need to allocate.
        void ClearBuffers()
        {
            _jsonBuffer.clear();
            _stringBuffer.clear();
        }

        String MsToFormattedString(uint32_t ms)
        {
            uint32_t seconds = ms / 1000;
            uint32_t minutes = seconds / 60;
            uint32_t hours = minutes / 60;
            seconds %= 60;
            minutes %= 60;
            String result = "";
            if (hours > 0)
                result += String(hours) + "h ";
            if (minutes > 0)
                result += String(minutes) + "m ";
            if (seconds > 0)
                result += String(seconds) + "s";
            result.trim();
            if (result.isEmpty())
                return "0s";
            return result;
        }

        bool Process()
        {
            SLocation location;
            ELocationSource source;
            _locationService->GetLocation(location, source);
            if (source == ELocationSource::LC_Invalid)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
                return false;
            }

            _jsonBuffer["trigger"] = GetConfig(int, trigger);

            _jsonBuffer["type"] = source;
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
                return false;
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

            return true;
        }

    protected:
        void RunServiceImpl() override
        {
            _locationService = GetService<Location>();
            _mqttService = GetService<MQTT>();

            #ifdef BATTERY_ADC
            _batteryService = GetService<Battery>();
            _batteryService->OnAfterSleep.Add([this](const Battery::ESleepType& sleepType) { _wakeEvent.Set(); });
            #endif

            _mqttService->WaitForConnection();

            #ifndef TEST_SLEEP
            //Encapsulate context.
            {
                GPS* gpsService = GetService<GPS>();
                if (!gpsService->IsUpdated())
                {
                    Process(); //Send a GSM location as a preliminary measure while waiting for the GPS location?
                    gpsService->WaitForLocation(pdMS_TO_TICKS(gpsService->GetPredictedTimeToFirstFix() * 1000)); //Increase runtime but try to get a GPS fix before continuing.
                }
            }
            #endif

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                ClearBuffers();

                Process();

                #ifdef BATTERY_ADC
                //TODO: Signal to the battery module to manage power.
                _batteryService->Sleep();
                //Wait for the wake event to be set before continuing, otherwise messages will be spammed as the sleep call is non-blocking.
                ReadieFur::Event::Waitable::WaitAny({ServiceCancellationToken.GetHandle(), &_wakeEvent});
                _wakeEvent.Clear();
                #else
                //TODO: Change these intervals to be dynamic.
                int interval = GetConfig(int, BATTERY_CHRG_INTERVAL);
                LOGD(nameof(Publish), "Sending next update in %s.", MsToFormattedString(interval).c_str());
                vTaskDelay(pdMS_TO_TICKS(interval));
                #endif
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
            AddDependencyType<GPS>();
            #ifdef BATTERY_ADC
            AddDependencyType<Battery>();
            #endif
        }
    };
};
