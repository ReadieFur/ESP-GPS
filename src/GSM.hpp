#pragma once

#include "Service/AService.hpp"
#include <freertos/FreeRTOSConfig.h>
#include "Board.h"
#include "Config.h"
#include <TinyGSM.h>
#include <TinyGsmClient.h>
#ifdef DEBUG
#include <StreamDebugger.hpp>
#endif
#include "Logging.hpp"
#include "Helpers.h"
#include "Storage.hpp"
#include <map>
#include <mutex>
#include "Event/ManualResetEvent.hpp"
#include "DebugStream.hpp"
#include <functional>
#include <queue>
#include "Event/CancellationToken.hpp"
#include <memory>
#include <freertos/task.h>
#include <utility>
#include <freertos/event_groups.h>
#ifdef BATTERY_ADC
#include "Battery.hpp"
#endif
#include <esp_system.h>

namespace ReadieFur::EspGps
{
    class GSM : public Service::AService
    {
    private:
        enum EActionState
        {
            Timeout = 1 << 1,
            Processing = 1 << 2,
            Processed = 1 << 3,
            Failed = 1 << 4
        };
        
        struct SAction
        {
            std::function<void()> action;
            uint32_t stackSize;
            EventGroupHandle_t eventGroup;
        };

        #ifdef DEBUG
        StreamDebugger* _debugger;
        #endif
        TinyGsm* _modem;
        std::mutex _mutex;
        std::map<int, TinyGsmClient*> _clients;
        Event::ManualResetEvent _modemReadyEvent;
        Event::ManualResetEvent _connectedEvent;
        TaskHandle_t _actionQueueTask;
        std::queue<SAction> _actionQueue;
        #ifdef BATTERY_ADC
        Battery* _batteryService;
        #endif

        #ifdef DEBUG
        void RefreshDebugStream()
        {
            #if false
            _debugger->DumpStream = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE && !_connectedEvent.IsSet() ? &DbgStream : nullptr;
            #elif false
            _debugger->DumpStream = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE ? &DbgStream : nullptr;
            #else
            _debugger->DumpStream = &DbgStream;
            #endif
        }
        #endif

        bool ValidateConnection()
        {
            _mutex.lock();

            if (_modem->testAT(500) && _modem->isNetworkConnected() && _modem->isGprsConnected())
            {
                _connectedEvent.Set();
                #ifdef DEBUG
                RefreshDebugStream();
                #endif
                _mutex.unlock();
                return true;
            }

            if (_connectedEvent.IsSet())
            {
                LOGW(nameof(GSM), "GSM disconnected...");
                _connectedEvent.Clear();
            }

            #ifdef DEBUG
            RefreshDebugStream();
            #endif

            //The check signal command in the source has no impact on the result so skip the unnecessary call.
            if (!_modem->waitForNetwork(10 * 1000, false))
            {
                LOGE(nameof(GSM), "Failed to reconnect to the network.");
                _mutex.unlock();
                return false;
            }

            //And make sure GPRS/EPS is still connected.
            if (_modem->isGprsConnected())
            {
                LOGI(nameof(GSM), "GSM reconnected.");
                _mutex.unlock();
                #ifdef DEBUG
                RefreshDebugStream();
                #endif
                return true;
            }

            const char *apn = GetConfig(const char*, MODEM_APN),
                *username = GetConfig(const char*, MODEM_USERNAME),
                *password = GetConfig(const char*, MODEM_PASSWORD);
            if (!_modem->gprsConnect(apn, username, password))
            {
                LOGE(nameof(GSM), "Failed to reconnect to GPRS.");
                _mutex.unlock();
                return false;
            }

            LOGI(nameof(GSM), "GSM reconnected.");
            _connectedEvent.Set();
            #ifdef DEBUG
            RefreshDebugStream();
            #endif
            _mutex.unlock();
            return true;
        }

        void SetupGPIO()
        {
            #ifdef BOARD_PWR
            pinMode(BOARD_PWR, OUTPUT);
            #endif
            #ifdef MODEM_POWERON
            pinMode(MODEM_POWERON, OUTPUT);
            #endif
            #ifdef MODEM_DTR
            pinMode(MODEM_DTR, OUTPUT);
            #endif
            #ifdef MODEM_PWRKEY
            pinMode(MODEM_PWRKEY, OUTPUT);
            #endif
            #ifdef MODEM_RING
            pinMode(MODEM_RING, INPUT_PULLUP);
            #endif
            #ifdef MODEM_RESET
            pinMode(MODEM_RESET, OUTPUT);
            #endif
            pinMode(MODEM_TX, OUTPUT);
            pinMode(MODEM_RX, INPUT_PULLDOWN);
        }

        void PowerOn()
        {
            MODEM_UART.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

            #ifdef MODEM_POWERON
            digitalWrite(MODEM_POWERON, HIGH);
            #endif

            #ifdef MODEM_RESET
            gpio_hold_dis((gpio_num_t)MODEM_RESET);
            digitalWrite(MODEM_RESET, !MODEM_RESET_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(100));
            digitalWrite(MODEM_RESET, MODEM_RESET_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(2600));
            digitalWrite(MODEM_RESET, !MODEM_RESET_LEVEL);
            #endif

            #ifdef MODEM_DTR
            digitalWrite(MODEM_DTR, LOW);
            #endif

            #ifdef MODEM_PWRKEY
            digitalWrite(MODEM_PWRKEY, LOW);
            vTaskDelay(pdMS_TO_TICKS(100));
            digitalWrite(MODEM_PWRKEY, HIGH);
            vTaskDelay(pdMS_TO_TICKS(300));
            digitalWrite(MODEM_PWRKEY, LOW);
            #endif

            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        void PowerOff()
        {
            _modemReadyEvent.Clear();
            _connectedEvent.Clear();

            if (_modem != nullptr)
            {
                // _modem->poweroff(); //This causes the device to power off but then reboot, so I will use the CFUN command instead to put the device into a low power state.
                _modem->setPhoneFunctionality(7, false);
                #ifdef MODEM_DTR
                digitalWrite(MODEM_DTR, HIGH);
                _modem->sleepEnable(true); //Requires a DTR signal to wake up.
                #endif
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            MODEM_UART.end();

            #ifdef MODEM_DTR
            digitalWrite(MODEM_DTR, HIGH);
            #endif

            #ifdef MODEM_POWERON
            digitalWrite(MODEM_POWERON, LOW);
            #endif

            #ifdef MODEM_RESET
            digitalWrite(MODEM_RESET, !MODEM_RESET_LEVEL);
            gpio_hold_en((gpio_num_t)MODEM_RESET);
            #endif
        }

        void ModemInit()
        {
            #if !defined(ALWAYS_FULL_REBOOT)
            switch (esp_reset_reason())
            {
            case ESP_RST_UNKNOWN: //If we reboot from an unknown state then we should restart the modem as it could be in a broken state.
            case ESP_RST_DEEPSLEEP: //Force soft-reset of the module if this is the first boot/wakeup from deep sleep (requires reset when exiting low power mode).
            case ESP_RST_POWERON: //In testing I can also reset manually from a state where the module is asleep so we will also reset in those cases.
                _modem->restart();
                vTaskDelay(pdMS_TO_TICKS(5000)); //Reboot takes about x seconds.
                break;
            default:
            //     _modem->restart();
            //     vTaskDelay(pdMS_TO_TICKS(5000)); //Reboot takes about x seconds.
                break;
            }
            #else
            //TODO: There is currently a bug I have not detected with the deep sleep of the module and restoration of it on power-up, so for now I will always reset the module on boot.
            _modem->restart();
            vTaskDelay(pdMS_TO_TICKS(5000));
            #endif

            if (!_modem->init())
            {
                LOGE(nameof(GSM), "Failed to start modem.");
                abort();
                return; //Does not return;
            }

            LOGD(nameof(GSM), "Modem info: %s", _modem->getModemInfo().c_str());

            //Unlock your SIM card with a PIN if needed.
            const char* pin = GetConfig(const char*, MODEM_PIN);
            if (pin && _modem->getSimStatus() != 3 && !_modem->simUnlock(pin))
            {
                LOGE(nameof(GSM), "Failed to unlock SIM.");
                abort();
                return;
            }

            _modemReadyEvent.Set();
        }

        static void ProcessActionQueue(void* param)
        {
            GSM* self = reinterpret_cast<GSM*>(param);
            char subtaskNameBuf[configMAX_TASK_NAME_LEN];

            while (!self->ServiceCancellationToken.IsCancellationRequested())
            {
                while (!self->ServiceCancellationToken.IsCancellationRequested() && !self->_actionQueue.empty())
                {
                    ulTaskNotifyTake(pdTRUE, 0);

                    self->WaitForConnection();

                    SAction actionObj = self->_actionQueue.front();
                    self->_actionQueue.pop();

                    EventBits_t bits = xEventGroupGetBits(actionObj.eventGroup);
                    if (bits & EActionState::Timeout)
                    {
                        //If a timeout has occurred then the QueueAction method will have exited leaving the event group active for this method so we must clean it up here.
                        vEventGroupDelete(actionObj.eventGroup);
                        portYIELD();
                        continue;
                    }
                    xEventGroupSetBits(actionObj.eventGroup, EActionState::Processing);
                    
                    sprintf(subtaskNameBuf, "gsmact%09d", xTaskGetTickCount());
                    TaskHandle_t subtaskHandle;
                    TaskFunction_t actionTask = [](void* subtaskParam)
                    {
                        SAction* subtaskActionObj = reinterpret_cast<SAction*>(subtaskParam);
                        subtaskActionObj->action();
                        xEventGroupSetBits(subtaskActionObj->eventGroup, EActionState::Processed);
                        vTaskDelete(NULL);
                    };

                    self->_mutex.lock();
                    //+x on the stack for the overhead required by the task wrapper.
                    if (xTaskCreate(actionTask, subtaskNameBuf, actionObj.stackSize + 32, &actionObj, self->ServiceEntrypointPriority, &subtaskHandle) != pdPASS)
                    {
                        self->_mutex.unlock();
                        xEventGroupSetBits(actionObj.eventGroup, EActionState::Failed);
                        portYIELD();
                        continue;
                    }

                    xEventGroupWaitBits(actionObj.eventGroup, EActionState::Processed, pdFALSE, pdFALSE, portMAX_DELAY);
                    self->_mutex.unlock();

                    portYIELD();
                }

                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }

            vTaskDelete(NULL);
        }

    protected:
        void RunServiceImpl() override
        {
            PowerOn();

            #ifdef DEBUG
            _debugger = new StreamDebugger(MODEM_UART, &DbgStream);
            _modem = new TinyGsm(*_debugger);
            RefreshDebugStream();
            #else
            _modem = new TinyGsm(MODEM_UART);
            #endif

            ModemInit();

            #ifdef BATTERY_ADC
            _batteryService = GetService<Battery>();
            _batteryService->OnBeforeSleep.Add([this](const Battery::ESleepType& sleepType)
            {
                switch (sleepType)
                {
                case Battery::ESleepType::Light:
                    _modemReadyEvent.Clear();
                    _connectedEvent.Clear();
                    #ifdef MODEM_DTR
                    digitalWrite(MODEM_DTR, HIGH);
                    _modem->sleepEnable(true);
                    #endif
                default:
                    break;
                }
            });
            _batteryService->OnAfterSleep.Add([this](const Battery::ESleepType& sleepType)
            {
                switch (sleepType)
                {
                case Battery::ESleepType::Light:
                    #ifdef MODEM_DTR
                    digitalWrite(MODEM_DTR, LOW);
                    _modem->sleepEnable(false);
                    #endif
                    _modemReadyEvent.Set();
                default:
                    break;
                }
            });
            #endif

            char actionQueueTaskNameBuf[configMAX_TASK_NAME_LEN];
            sprintf(actionQueueTaskNameBuf, "gsm%012d", xTaskGetTickCount());
            if (xTaskCreate(ProcessActionQueue, actionQueueTaskNameBuf, IDLE_TASK_STACK_SIZE + 1024, this, ServiceEntrypointPriority, &_actionQueueTask) != pdPASS)
            {
                LOGE(nameof(GSM), "Failed to create action queue task.");
                return;
            }

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                ValidateConnection();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            PowerOff();

            delete _modem;
            _modem = nullptr;
            #ifdef DEBUG
            delete _debugger;
            _debugger = nullptr;
            #endif
            #ifdef BATTERY_ADC
            _batteryService = nullptr;
            #endif
        }

    public:
        GSM()
        {
            ServiceEntrypointStackDepth += 2048;
            ServiceEntrypointPriority = configMAX_PRIORITIES * 0.4;
            SetupGPIO();
            #ifdef BATTERY_ADC
            AddDependencyType<Battery>();
            #endif
        }

        //TODO: Return a custom client object that puts requests in a queue.
        TinyGsmClient* CreateClient()
        {
            _mutex.lock();

            if (_modem == nullptr)
                abort(); //Not setup.

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
                LOGW(nameof(GSM), "Maximum GSM clients reached.");
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

        bool WaitForConnection(TickType_t timeout = portMAX_DELAY)
        {
            return _connectedEvent.WaitOne(timeout);
        }

        bool WaitForModem(TickType_t timeout = portMAX_DELAY)
        {
            return _modemReadyEvent.WaitOne(timeout);
        }

        //From my testing I have found that asynchronous communication causes errors on the GSM module, so instead we will create an action queue.
        //TODO: Make this "awaitable" instead of always blocking.
        bool QueueAction(std::function<void()> action, uint32_t stackSize = IDLE_TASK_STACK_SIZE, TickType_t timeout = portMAX_DELAY)
        {
            //Don't mutex lock here.

            if (_actionQueueTask == nullptr)
                abort(); //Not setup.

            SAction actionObj = SAction
            {
                .action = action,
                .stackSize = stackSize,
                .eventGroup = xEventGroupCreate()
            };
            if (actionObj.eventGroup == NULL)
            {
                LOGE(nameof(GSM), "Failed to queue action, out of memory.");
                return false;
            }

            _actionQueue.push(actionObj);
            xTaskNotifyGive(_actionQueueTask);

            EventBits_t bits = xEventGroupWaitBits(actionObj.eventGroup, EActionState::Processing, pdTRUE, pdFALSE, timeout);
            if ((!bits & EActionState::Processing))
            {
                xEventGroupSetBits(actionObj.eventGroup, EActionState::Timeout);
                //Let the task processor delete the event group.
                return false;
            }

            bits = xEventGroupWaitBits(actionObj.eventGroup, EActionState::Processed | EActionState::Failed, pdFALSE, pdFALSE, portMAX_DELAY);
            vEventGroupDelete(actionObj.eventGroup);

            return bits & EActionState::Processed;
        }

        TinyGsm* GetModem()
        {
            return _modem;
        }

        std::mutex* GetModemMutex()
        {
            return &_mutex;
        }

        // bool Restart()
        // {
        //     _mutex.lock();
        //     bool retVal = _modem->restart();
        //     _mutex.unlock();
        //     return retVal;
        // }

        // bool GetLocation(float* lat = nullptr, float* lng = nullptr, float* acc = nullptr,
        //     int* year = nullptr, int* month = nullptr, int* day = nullptr,
        //     int* hour = nullptr, int* minute = nullptr, int* second = nullptr,
        //     TickType_t timeout = portMAX_DELAY)
        // {
        //     if (!_connectedEvent.WaitOne(timeout))
        //         return false;
        //     _mutex.lock();
        //     bool retVal = _modem->getGsmLocation(lat, lng, acc, year, month, day, hour, minute, second);
        //     _mutex.unlock();
        //     return retVal;
        // }
    };
};
