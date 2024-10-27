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
#include "Diagnostic/DiagnosticsService.hpp"
#include "MQTT.hpp"
#include <esp_check.h>
#include "Location.hpp"
#include "Publish.hpp"

#define CHECK_SERVICE_RESULT(func) do {                                     \
        Service::EServiceResult result = func;                              \
        if (result == Service::Ok) break;                                   \
        LOGE(pcTaskGetName(NULL), "Failed with result: %i", result);        \
        abort();                                                            \
    } while (0)

using namespace ReadieFur;

void setup()
{
    #ifdef DEBUG
    // esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set("*", ESP_LOG_DEBUG);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif

    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::SerialMonitor>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::SerialMonitor>());

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<Diagnostic::DiagnosticsService>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<Diagnostic::DiagnosticsService>());
    #endif

    EspGps::Storage::Init();
    EspGps::Motion::Configure();

    #ifdef BATTERY_ADC
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::Battery>());
    #endif
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::GPS>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::GSM>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::Location>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::MQTT>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::InstallService<EspGps::Publish>());

    gpio_deep_sleep_hold_en();

    #ifdef BATTERY_ADC
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::Battery>());
    #endif
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::GPS>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::GSM>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::Location>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::MQTT>());
    CHECK_SERVICE_RESULT(Service::ServiceManager::StartService<EspGps::Publish>());
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
