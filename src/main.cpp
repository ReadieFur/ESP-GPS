#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "Board.h"
#include "Config.h"
#include "Service/ServiceManager.hpp"
#include "SerialMonitor.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "Motion.hpp"
#include "Storage.hpp"
#include "Battery.hpp"
#include "Diagnostics.hpp"

using namespace ReadieFur;

void setup()
{
    Service::ServiceManager::InstallService<EspGps::SerialMonitor>();
    #ifdef DEBUG
    Service::ServiceManager::InstallService<EspGps::Diagnostics>();
    Service::ServiceManager::StartService<EspGps::Diagnostics>();
    #endif

    EspGps::Storage::Init();
    EspGps::Motion::Configure();

    #ifdef BATTERY_ADC
    Service::ServiceManager::InstallService<EspGps::Battery>();
    #endif
    Service::ServiceManager::InstallService<EspGps::GPS>();
    Service::ServiceManager::InstallService<EspGps::GSM>();

    Service::ServiceManager::StartService<EspGps::SerialMonitor>();
    #ifdef BATTERY_ADC
    Service::ServiceManager::StartService<EspGps::Battery>();
    #endif
    Service::ServiceManager::StartService<EspGps::GPS>();
    Service::ServiceManager::StartService<EspGps::GSM>();
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
