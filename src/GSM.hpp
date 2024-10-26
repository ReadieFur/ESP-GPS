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

namespace ReadieFur::EspGps
{
    class GSM : public Service::AService
    {
    private:
        enum EActionState
        {
            Waiting,
            Processing,
            Processed,
            Failed
        };

        struct SAction
        {
            std::function<void()> action;
            Event::CancellationTokenSource cts;
            EActionState state = EActionState::Waiting;
            uint32_t stackSize;
        };

        #ifdef DEBUG
        StreamDebugger* _debugger;
        #endif
        TinyGsm* _modem;
        std::mutex _mutex;
        std::map<int, TinyGsmClient*> _clients;
        Event::ManualResetEvent _connectedEvent;
        TaskHandle_t _actionQueueTask;
        std::queue<std::shared_ptr<SAction>> _actionQueue;

        #ifdef DEBUG
        void RefreshDebugStream()
        {
            #if true
            _debugger->DumpStream = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE && !_connectedEvent.IsSet() ? &DbgStream : nullptr;
            #else
            _debugger->DumpStream = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE ? &DbgStream : nullptr;
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
            pinMode(MODEM_RESET, OUTPUT);
            pinMode(MODEM_TX, OUTPUT);
            pinMode(MODEM_RX, INPUT_PULLDOWN);
        }

        void PowerOn()
        {
            Serial2.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

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
        }

        void PowerOff()
        {
            if (_modem != nullptr)
            {
                _modem->poweroff();
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            Serial2.end();

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

        static void ProcessActionQueue(void* param)
        {
            GSM* self = reinterpret_cast<GSM*>(param);

            while (!self->ServiceCancellationToken.IsCancellationRequested())
            {
                while (!self->ServiceCancellationToken.IsCancellationRequested() && !self->_actionQueue.empty())
                {
                    ulTaskNotifyTake(pdTRUE, 0);

                    self->WaitForConnection();

                    std::shared_ptr<SAction> actionObj = self->_actionQueue.front();

                    if (actionObj->cts.IsCancelled())
                    {
                        self->_actionQueue.pop();
                        portYIELD(); //Allow other higher priority tasks to run.
                        continue;
                    }

                    TaskFunction_t actionTask = [](void* taskParam)
                    {
                        auto taskParamsLocal = *reinterpret_cast<std::pair<Event::ManualResetEvent, std::shared_ptr<SAction>>*>(taskParam);
                        taskParamsLocal.second->action();
                        taskParamsLocal.first.Set();
                        vTaskDelete(NULL);
                    };

                    std::pair<Event::ManualResetEvent, std::shared_ptr<SAction>> taskParams;
                    taskParams.second = actionObj;
                    char subtaskNameBuf[configMAX_TASK_NAME_LEN];
                    sprintf(subtaskNameBuf, "gsmact%09d", xTaskGetTickCount());
                    TaskHandle_t subtaskHandle;

                    self->_mutex.lock();
                    if (xTaskCreate(actionTask, subtaskNameBuf, actionObj->stackSize, &taskParams, self->ServiceEntrypointPriority, &subtaskHandle) != pdPASS)
                    {
                        self->_mutex.unlock();
                        actionObj->state = EActionState::Failed;
                        self->_actionQueue.pop();
                        portYIELD();
                        continue;
                    }

                    //TODO: Pay attention to the cts here.
                    actionObj->state = EActionState::Processing;
                    taskParams.first.WaitOne(portMAX_DELAY);
                    self->_mutex.unlock();
                    actionObj->state = EActionState::Processed;
                    actionObj->cts.Cancel();

                    self->_actionQueue.pop();
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
            _debugger = new StreamDebugger(Serial2, &DbgStream);
            _modem = new TinyGsm(*_debugger);
            RefreshDebugStream();
            #else
            _modem = new TinyGsm(Serial2);
            #endif

            vTaskDelay(pdMS_TO_TICKS(2000));
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

            char actionQueueTaskNameBuf[configMAX_TASK_NAME_LEN];
            sprintf(actionQueueTaskNameBuf, "gsm%012d", xTaskGetTickCount());
            if (xTaskCreate(ProcessActionQueue, actionQueueTaskNameBuf, configIDLE_TASK_STACK_SIZE + 1024, this, ServiceEntrypointPriority, &_actionQueueTask) != pdPASS)
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
        }

    public:
        //TODO: Create an event that can be called by the owning thread and not this thread.

        GSM()
        {
            ServiceEntrypointStackDepth += 1024;
            ServiceEntrypointPriority = configMAX_PRIORITIES * 0.4;
            SetupGPIO();
        }

        ~GSM()
        {
            PowerOff();

            if (_modem != nullptr)
                delete _modem;
            _modem = nullptr;

            if (_debugger != nullptr)
                delete _debugger;
            _debugger = nullptr;
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

        //From my testing I have found that asynchronous communication causes errors on the GSM module, so instead we will create an action queue.
        //TODO: Possibly pass a stack size to be used for this task.
        bool QueueAction(std::function<void()> action, TickType_t timeout = portMAX_DELAY, uint32_t stackSize = configIDLE_TASK_STACK_SIZE)
        {
            //Don't mutex lock here.

            if (_actionQueueTask == nullptr)
                abort(); //Not setup.

            //TODO: Switch to a different solution here as I believe this shared pointer is causing a memory leak.
            std::shared_ptr<SAction> actionObj = std::make_shared<SAction>();
            if (actionObj == nullptr)
            {
                LOGE(nameof(GSM), "Failed to queue action, out of memory.");
                return false;
            }

            actionObj->action = action;
            actionObj->stackSize = stackSize;
            actionObj->cts.CancelAfter(timeout);

            _actionQueue.push(actionObj);
            xTaskNotifyGive(_actionQueueTask);

            actionObj->cts.GetToken().WaitForCancellation();

            return actionObj->state == EActionState::Processed;
        }
    };
};
