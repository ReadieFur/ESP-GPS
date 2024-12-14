#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logging.hpp"
#include "Board.h"
#include "Config.h"
#include "Service/ServiceManager.hpp"
#include "SerialMonitor.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "Motion.hpp"
#include "Storage.hpp"
#include "Battery.hpp"
#ifdef DEBUG
#include "Diagnostic/DiagnosticsService.hpp"
#endif
#include "MQTT.hpp"
#include <esp_check.h>
#include "Location.hpp"
#include "Publish.hpp"
#include "Checkpoint.hpp"

#define CHECK_SERVICE_RESULT(func) do {                                     \
        ReadieFur::Service::EServiceResult result = func;                   \
        if (result == ReadieFur::Service::Ok) break;                        \
        LOGE(pcTaskGetName(NULL), "Failed with result: %i", result);        \
        abort();                                                            \
    } while (false)

using namespace ReadieFur::EspGps;

void CheckWakeupReason()
{
    esp_sleep_source_t wakeupCause = esp_sleep_get_wakeup_cause();
    auto lastTrigger = Storage::Cache["trigger"];
    if (lastTrigger.isNull())
    {
        //If the last trigger is unset then the last run should be considered successful.
        Storage::Cache["trigger"] = wakeupCause;
        if (!Storage::Save())
        {
            LOGE(pcTaskGetName(NULL), "Failed to save wakeup reason.");
            // abort();
        }
        return;
    }

    //Otherwise if the last run failed, keep the trigger as the old value...
    //But only if the current wakeup trigger is less important than the old one.
    switch (wakeupCause)
    {
    //The following take priority over the previous trigger.
    case ESP_SLEEP_WAKEUP_EXT0: //Interrupt.
    case ESP_SLEEP_WAKEUP_TOUCHPAD: //TODO: Voltage change.
        Storage::Cache["trigger"] = wakeupCause;
        if (!Storage::Save())
        {
            LOGE(pcTaskGetName(NULL), "Failed to save wakeup reason.");
            // abort();
        }
        return;
    //Otherwise keep the trigger as the old one (as we will retry the failed trigger).
    default:
        return;
    }
}

void setup()
{
    #ifdef DEBUG
    // esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set("*", ESP_LOG_DEBUG);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif

    Storage::Init();
    #ifdef MPU_INT
    // Motion::Configure();
    #endif

    CheckWakeupReason();

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<SerialMonitor>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<SerialMonitor>());

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<ReadieFur::Diagnostic::DiagnosticsService>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    #ifdef BATTERY_ADC
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Battery>());
    #endif
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GPS>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Location>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<MQTT>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Publish>());

    gpio_deep_sleep_hold_en();

    #ifdef BATTERY_ADC
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Battery>());
    #endif
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GPS>());

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GSM>());
    GSM* _gsmService = ReadieFur::Service::ServiceManager::GetService<GSM>();
    _gsmService->WaitForConnection(pdMS_TO_TICKS(20 * 1000));

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Location>());

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<MQTT>());
    _gsmService->WaitForConnection(pdMS_TO_TICKS(10 * 1000));

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Publish>());
}

void loop()
{
    vTaskDelete(NULL);
}

#ifndef ARDUINO
extern "C" void app_main()
{
    setup();
    while (true)
        if (eTaskGetState(NULL) != eTaskState::eDeleted)
            loop();
}
#endif
