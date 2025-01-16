#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logging.hpp"
#include "Board.h"
#include "Config.h"

#ifdef DEBUG
// #define TEST_DELAY_ENABLE
// #define TEST_GPS
// #define TEST_MODEM
// #define TEST_MQTT
// #define TEST_BATTERY
// #define TEST_SLEEP
// #define TEST_MISC
#endif

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
#include "Network/WiFi/Modem.hpp"
#include "Network/WiFi/EspNow.hpp"
#include "Network/WiFi/OTA.hpp"
#include <esp_system.h>

#define CHECK_SERVICE_RESULT(func) do {                                                 \
        ReadieFur::Service::EServiceResult result = func;                               \
        if (result == ReadieFur::Service::Ok) break;                                    \
        LOGE("main", "[%d] Failed with result: %i", __LINE__, result);                  \
        abort();                                                                        \
    } while (0)

#define CHECK_ESP_RESULT(func) do {                                                     \
        esp_err_t result = func;                                                        \
        if (result == ESP_OK) break;                                                    \
        LOGE("main", "[%d] Failed with result: %s", __LINE__, esp_err_to_name(result)); \
        abort();                                                                        \
    } while (0)

using namespace ReadieFur::EspGps;

#ifdef DEBUG
bool DoTests()
{
    #if defined(TEST_DELAY_ENABLE)
    vTaskDelay(pdMS_TO_TICKS(2000));
    #endif
    #if defined(TEST_GPS)
    #ifdef GPS_INTEGRATED
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GSM>());
    return true;
    #endif
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GPS>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Location>());
    return true;
    #elif defined(TEST_MODEM)
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GSM>());
    GSM* gsmService = ReadieFur::Service::ServiceManager::GetService<GSM>();
    gsmService->WaitForModem(portMAX_DELAY);
    return true;
    #elif defined(TEST_MQTT)
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<MQTT>());
    return true;
    #elif defined(TEST_BATTERY)
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Battery>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GSM>());
    ReadieFur::Service::ServiceManager::GetService<GSM>()->WaitForModem();
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GPS>());
    Battery* batteryService = ReadieFur::Service::ServiceManager::GetService<Battery>();
    // batteryService->DoSystemManagement = true;
    batteryService->Sleep();
    return true;
    #elif defined(TEST_MISC)
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Battery>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Motion>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GSM>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<GPS>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Location>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<MQTT>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<Publish>());
    std::vector<std::type_index> services = ReadieFur::Service::ServiceManager::GetServices();
    LOGD("main", "Service count: %i", services.size());
    for (auto &&service : services)
        LOGI("main", "Service: %s", service.name());
    HALT();
    return true;
    #endif
    return false;
}
#endif

void CheckWakeupReason(esp_reset_reason_t& resetReason, esp_sleep_source_t& wakeupSource)
{
    resetReason = esp_reset_reason();
    wakeupSource = esp_sleep_get_wakeup_cause();

    LOGD("main", "Reset reason: %i, Wakeup source: %i", resetReason, wakeupSource);

    auto lastTrigger = Storage::Cache["trigger"];
    if (lastTrigger.isNull())
    {
        //If the last trigger is unset then the last run should be considered successful.
        Storage::Cache["trigger"] = wakeupSource;
        if (!Storage::Save())
        {
            LOGE("main", "Failed to save wakeup reason.");
            // abort();
        }
        return;
    }

    //Otherwise if the last run failed, keep the trigger as the old value...
    //But only if the current wakeup trigger is less important than the old one.
    switch (wakeupSource)
    {
    //The following take priority over the previous trigger.
    case ESP_SLEEP_WAKEUP_EXT0: //Interrupt.
    case ESP_SLEEP_WAKEUP_TOUCHPAD: //TODO: Voltage change.
    case ESP_SLEEP_WAKEUP_GPIO: //Pin.
        Storage::Cache["trigger"] = wakeupSource;
        if (!Storage::Save())
        {
            LOGE("main", "Failed to save wakeup reason.");
            // abort();
        }
        break;
    //Otherwise keep the trigger as the old one (as we will retry the failed trigger).
    default:
        return;
    }
}

void setup()
{
    #ifdef DEBUG
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<SerialMonitor>());

    #if defined(DEBUG) && true
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    Storage::Init();

    #ifdef DEBUG
    if (DoTests())
        return; //Tests should be independent of the main system, so return when they are done.
    #endif

    esp_reset_reason_t resetReason;
    esp_sleep_source_t wakeupSource;
    CheckWakeupReason(resetReason, wakeupSource);

    #ifdef BATTERY_ADC
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Battery>());
    Battery* batteryService = ReadieFur::Service::ServiceManager::GetService<Battery>();
    /* The exit deep sleep event won't be fired here as the other components won't be ready to receive it yet.
     * We can enable the system management though as the external components should already be configured in their deep sleep state (meaning we can go back to sleep again right away if needs be).
     */
    switch (resetReason)
    {
    case ESP_RST_DEEPSLEEP:
        batteryService->DoSystemManagement = true;
        break;
    default:
        //Default is false.
        break;
    }
    #endif

    #ifdef MPU_INT
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Motion>());
    #endif

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GSM>());
    GSM* gsmService = ReadieFur::Service::ServiceManager::GetService<GSM>();
    gsmService->WaitForModem(); //Will fail internally if the modem does not respond after a certain amount of time (desired behaviour).

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<GPS>());
    // GPS* gpsService = ReadieFur::Service::ServiceManager::GetService<GPS>();
    // gpsService->WaitForLocation(pdMS_TO_TICKS(gpsService->GetPredictedTimeToFirstFix() * 1000)); //Increase runtime but try to get a GPS fix before continuing.

    gsmService->WaitForConnection(pdMS_TO_TICKS(20 * 1000));
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Location>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<MQTT>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Publish>());

    #if defined(ENABLE_ESPNOW) || defined(ENABLE_OTA)
    ReadieFur::Network::WiFi::Modem::Init();
    wifi_config_t apConfig =
    {
        .ap =
        {
            .ssid_len = sizeof(AP_SSID),
            .channel = 1,
            .ssid_hidden = true,
            .beacon_interval = 100,
        }
    };
    memcpy(apConfig.ap.ssid, AP_SSID, sizeof(AP_SSID));
    ReadieFur::Network::WiFi::Modem::ConfigureInterface(WIFI_IF_AP, apConfig);
    #endif

    #ifdef ENABLE_ESPNOW
    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::EspNow::Init());
    ReadieFur::Network::WiFi::EspNow::SetPowerSaving(BATTERY_CHRG_INTERVAL);
    #endif

    #ifdef ENABLE_OTA
    httpd_config_t otaHttpdConfig = HTTPD_DEFAULT_CONFIG();
    otaHttpdConfig.task_priority = tskIDLE_PRIORITY + 5;
    otaHttpdConfig.server_port = 81;
    otaHttpdConfig.ctrl_port += 1;
    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::OTA::Init(&otaHttpdConfig));
    #endif

    LOGI("main", "Setup complete.");

    #ifdef BATTERY_ADC
    //Only enable the battery system management now that all the components are ready.
    batteryService->DoSystemManagement = true;
    #endif
}

void loop()
{
    vTaskDelete(NULL);
}

#ifndef ARDUINO
extern "C" void app_main()
{
    TaskHandle_t mainTaskHandle = xTaskGetCurrentTaskHandle();
    setup();
    while (eTaskGetState(mainTaskHandle) != eTaskState::eDeleted)
        loop();
}
#endif
