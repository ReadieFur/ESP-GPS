#pragma once

#include "Service/AService.hpp"
#include <freertos/FreeRTOSConfig.h>
#include "Board.h"
#include "Config.h"
#include <HardwareSerial.h>
#include <TinyGSM.h>
#include <TinyGsmClient.h>
#ifdef DEBUG
#include <StreamDebugger.hpp>
#endif
#include <esp_log.h>
#include "Helpers.h"
#include "Storage.hpp"
#include <map>
#include <mutex>
#include "Event/ManualResetEvent.hpp"

namespace ReadieFur::EspGps
{
    class GSM : public Service::AService
    {
    private:
        #ifdef DEBUG
        StreamDebugger* _debugger;
        #endif
        TinyGsm* _modem;
        std::mutex _mutex;
        std::map<int, TinyGsmClient*> _clients;
        Event::ManualResetEvent _connectedEvent;

        #ifdef DEBUG
        void RefreshDebugStream()
        {
            // _debugger->DumpStream = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE ? &Serial : nullptr;
        }
        #endif

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

        bool ValidateConnection()
        {
            _mutex.lock();

            if (_modem->testAT(500) && _modem->isNetworkConnected() && _modem->isGprsConnected())
            {
                // _connectedEvent.Set(); //Should already be set.
                _mutex.unlock();
                return true;
            }

            if (_connectedEvent.IsSet())
            {
                ESP_LOGW(nameof(GSM), "GSM disconnected...");
                _connectedEvent.Clear();
            }

            //The check signal command in the source has no impact on the result so skip the unnecessary call.
            if (!_modem->waitForNetwork(10 * 1000, false))
            {
                ESP_LOGE(nameof(GSM), "Failed to reconnect to the network.");
                _mutex.unlock();
                return false;
            }

            //And make sure GPRS/EPS is still connected.
            if (_modem->isGprsConnected())
            {
                ESP_LOGI(nameof(GSM), "GSM reconnected.");
                _mutex.unlock();
                return true;
            }

            const char *apn = GetConfig(const char*, MODEM_APN),
                *username = GetConfig(const char*, MODEM_USERNAME),
                *password = GetConfig(const char*, MODEM_PASSWORD);
            if (!_modem->gprsConnect(apn, username, password))
            {
                ESP_LOGE(nameof(GSM), "Failed to reconnect to GPRS.");
                _mutex.unlock();
                return false;
            }

            ESP_LOGI(nameof(GSM), "GSM reconnected.");
            _connectedEvent.Set();
            _mutex.unlock();
            return true;
        }

    protected:
        void RunServiceImpl() override
        {
            Serial2.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
            #ifdef DEBUG
            _debugger = new StreamDebugger(Serial2, &Serial);
            _modem = new TinyGsm(*_debugger);
            RefreshDebugStream();
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
            {
                ESP_LOGE(nameof(GSM), "Failed to start modem.");
                abort();
                return; //Does not return;
            }

            _modem->getModemInfo(); //In debug mode the output of this is relayed through the stream debugger, so no need to manually log it.

            //Unlock your SIM card with a PIN if needed.
            const char* pin = GetConfig(const char*, MODEM_PIN);
            if (pin && _modem->getSimStatus() != 3 && !_modem->simUnlock(pin))
            {
                ESP_LOGE(nameof(GSM), "Failed to unlock SIM.");
                abort();
                return;
            }

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                #ifdef DEBUG
                RefreshDebugStream();
                #endif

                ValidateConnection();

                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

    public:
        //TODO: Create an event that can be called by the owning thread and not this thread.

        GSM()
        {
            ServiceEntrypointStackDepth += 1024;
            ServiceEntrypointPriority = configMAX_PRIORITIES * 0.4;
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

        TinyGsmClient* CreateClient()
        {
            _mutex.lock();

            //Get first free MUX ID.
            int mux = 0;
            //Iterate through the map in order (std::map is sorted apparently), looking for gaps in the keys.
            for (const auto& kvp : _clients)
            {
                if (kvp.first != mux)
                {
                    //We've found a gap, return the missing number.
                    break;
                }
                mux++; //Keep checking the next number.
            }

            if (mux > TINY_GSM_MUX_COUNT - 1)
            {
                _mutex.unlock();
                ESP_LOGW(nameof(GSM), "Maximum GSM clients reached.");
                return nullptr;
            }

            TinyGsmClient* client = new TinyGsmClient(*_modem, mux);
            _clients[mux] = client;

            _mutex.unlock();
            return client;
        }

        void DestroyClient(TinyGsmClient* client)
        {
            if (client == nullptr)
                return;

            _mutex.lock();

            for (auto it = _clients.begin(); it != _clients.end(); ++it)
            {
                if (it->second == client)
                {
                    _clients.erase(it);
                    break;
                }
            }

            delete client;

            _mutex.unlock();
        }

        bool IsConnected()
        {
            #if true
            return _connectedEvent.IsSet();
            #else
            if (!_mutex.try_lock())
            {
                //If we failed to lock then it is likely that the connection is being tested above, so just use the cached value for now.
                return _connectedEvent.IsSet();
            }

            _mutex.lock();
            if (_modem->testAT(500) && _modem->isNetworkConnected() && _modem->isGprsConnected())
            {
                _mutex.unlock();
                return true;
            }

            _connectedEvent.Clear();

            _mutex.unlock();
            return false;
            #endif
        }

        bool WaitForConnection(TickType_t timeout = portMAX_DELAY)
        {
            return _connectedEvent.WaitOne(timeout);
        }
    };
};
