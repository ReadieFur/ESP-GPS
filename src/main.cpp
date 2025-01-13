#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logging.hpp"
#include "Board.h"
#include "Config.h"

#include "Service/ServiceManager.hpp"
#include "SerialMonitor.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#ifdef MPU_INT
#include "Motion.hpp"
#endif
#include "Storage.hpp"
#ifdef BATTERY_ADC
#include "Battery.hpp"
#endif
#ifdef DEBUG
#include "Diagnostic/DiagnosticsService.hpp"
#endif
#include "MQTT.hpp"
#include <esp_check.h>
#include "Location.hpp"
#include "Publish.hpp"
#include "Checkpoint.hpp"
#include "Network/WiFi/EspNow.hpp"

#ifdef DEBUG
// #define TEST_GPS
// #define TEST_MQTT
#endif

#define CHECK_SERVICE_RESULT(func) do {                                                 \
        ReadieFur::Service::EServiceResult result = func;                               \
        if (result == ReadieFur::Service::Ok) break;                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %i", __LINE__, result);     \
        abort();                                                                        \
    } while (0)

#define CHECK_ESP_RESULT(func) do {                                                     \
        esp_err_t result = func;                                                        \
        if (result == ESP_OK) break;                                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %s", __LINE__, esp_err_to_name(result));   \
        abort();                                                                        \
    } while (0)

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
    case ESP_SLEEP_WAKEUP_GPIO: //Pin.
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
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    // esp_log_level_set("*", ESP_LOG_DEBUG);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif

    #if defined(DEBUG) && false
    vTaskDelay(pdMS_TO_TICKS(2000));
    #endif

    Storage::Init();

    #ifdef MPU_INT
    if (!Motion::Configure())
    {
        LOGE(pcTaskGetName(NULL), "Failed to configure motion sensor.");
        abort();
    }
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

    #if defined(TEST_GPS)
    #ifdef GPS_INTEGRATED
    esp_log_level_set(nameof(GSM), ESP_LOG_VERBOSE);
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GSM>());
    #endif
    esp_log_level_set(nameof(GPS), ESP_LOG_VERBOSE);
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GPS>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GPS>());
    // esp_log_level_set(nameof(Location), ESP_LOG_VERBOSE);
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Location>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Location>());
    return;
    #elif defined(TEST_MQTT)
    esp_log_level_set(nameof(GSM), ESP_LOG_VERBOSE);
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GSM>());
    esp_log_level_set(nameof(MQTT), ESP_LOG_VERBOSE);
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<MQTT>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<MQTT>());
    return;
    #endif

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GPS>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Location>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<MQTT>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Publish>());

    gpio_deep_sleep_hold_en();

    #ifdef BATTERY_ADC
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Battery>());
    #endif

    #ifdef GPS_INTEGRATED
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GPS>());
    #else
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GPS>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<GSM>());
    #endif
    GSM* _gsmService = ReadieFur::Service::ServiceManager::GetService<GSM>();
    _gsmService->WaitForConnection(pdMS_TO_TICKS(20 * 1000));

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Location>());

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<MQTT>());
    _gsmService->WaitForConnection(pdMS_TO_TICKS(10 * 1000));

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<Publish>());

    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::EspNow::Init()); //TODO: Move to own service file, just here for init testing.
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
