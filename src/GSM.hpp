#pragma once

#include "Service/AService.hpp"
#include <freertos/FreeRTOSConfig.h>
#include "Board.h"
#include "Config.h"
#include <HardwareSerial.h>
#include <TinyGSM.h>
#include <TinyGsmClient.h>
#ifdef DEBUG
#include <StreamDebugger.h>
#endif
#include <esp_log.h>
#include "Helpers.h"

namespace ReadieFur::EspGps
{
    class GSM : public Service::AService
    {
    private:
        #ifdef DEBUG
        StreamDebugger* _debugger;
        #endif
        TinyGsm* _modem;

        bool HardResetModem()
        {
            #ifdef MODEM_POWERON
            digitalWrite(MODEM_POWERON, LOW);
            vTaskDelay(pdMS_TO_TICKS(500));
            digitalWrite(MODEM_POWERON, HIGH);
            #endif

            digitalWrite(MODEM_RESET, LOW); //TODO: Check if I need to change this?

            #ifdef MODEM_PWRKEY
            digitalWrite(MODEM_PWRKEY, LOW);
            vTaskDelay(pdMS_TO_TICKS(100));
            digitalWrite(MODEM_PWRKEY, HIGH);
            vTaskDelay(pdMS_TO_TICKS(1000));
            digitalWrite(MODEM_PWRKEY, LOW);
            #endif

            //Wait for modem to be ready.
            vTaskDelay(pdMS_TO_TICKS(3000));

            return _modem->init();
        }

    protected:
        void RunServiceImpl() override
        {
            Serial2.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
            #ifdef DEBUG
            _debugger = new StreamDebugger(Serial2, Serial);
            _modem = new TinyGsm(*_debugger);
            #else
            _modem = new TinyGsm(Serial2);
            #endif

            #ifdef MODEM_POWERON
            pinMode(MODEM_POWERON, OUTPUT);
            #endif
            pinMode(MODEM_RESET, OUTPUT);
            #ifdef MODEM_PWRKEY
            pinMode(MODEM_PWRKEY, OUTPUT);
            #endif
            #ifdef MODEM_RING
            pinMode(MODEM_RING, INPUT_PULLUP);
            #endif

            if (!HardResetModem())
                return; //TODO: Log here, program should crash at this stage if this is reached.

            _modem->getModemInfo(); //In debug mode the output of this is relayed through the stream debugger, so no need to manually log it.

            vTaskDelay(portMAX_DELAY);
        }

    public:
        GSM()
        {
            ServiceEntrypointStackDepth += 4096;
        }

        ~GSM()
        {
            if (_modem != nullptr)
                delete _modem;
            _modem = nullptr;

            if (_debugger != nullptr)
                delete _debugger;
            _debugger = nullptr;
        }
    };
};
