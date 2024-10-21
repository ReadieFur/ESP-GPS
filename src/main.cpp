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

using namespace ReadieFur;

void setup()
{
    Service::ServiceManager::InstallService<EspGps::SerialMonitor>();

    EspGps::Storage::Init();
    EspGps::Motion::Configure();
    
    Service::ServiceManager::InstallService<EspGps::GPS>();
    Service::ServiceManager::InstallService<EspGps::GSM>();

    Service::ServiceManager::StartService<EspGps::SerialMonitor>();
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
