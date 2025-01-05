#pragma once

#include "Service/AService.hpp"
#include <PubSubClient.h>
#include <Wstring.h>
#include "Storage.hpp"
#include "Logging.hpp"
#include "Helpers.h"
#include "GSM.hpp"
#include <TinyGsmClient.h>

namespace ReadieFur::EspGps
{
    //TODO: Use the MQTT service provided by the GSM module if supported.
    class MQTT : public Service::AService
    {
    private:
        GSM* _gsmService = nullptr;
        TinyGsmClient* _gsmClient = nullptr;
        PubSubClient _mqtt;
        String _publishTopic;
        String _subscribeTopic;
        Event::ManualResetEvent _connectedEvent;

        static void Callback(char* topic, byte* payload, uint len)
        {
            String message = String(reinterpret_cast<const char*>(payload), len);
            LOGV(nameof(MQTT), "Message arrived:\n%s", message.c_str());
            //TODO: Send to API.
        }

        bool ValidateConnection()
        {
            //No need to check if GSM is connected as this callback will only be run if it is connected.
            if (_mqtt.connected())
                return true;

            if (_connectedEvent.IsSet())
            {
                LOGW(nameof(MQTT), "Disconnected from MQTT server...");
                _connectedEvent.Clear();
            }

            //Connect to MQTT broker.
            if (!_mqtt.connect(GetConfig(const char*, MQTT_CLIENT_ID),
                GetConfig(const char*, MQTT_USERNAME), GetConfig(const char*, MQTT_PASSWORD)))
            {
                LOGE(nameof(MQTT), "Failed to connect to MQTT server...");
                return false;
            }

            _mqtt.subscribe(_subscribeTopic.c_str());

            LOGI(nameof(MQTT), "MQTT reconnected.");
            _connectedEvent.Set();
            return true;
        }

    protected:
        void RunServiceImpl() override
        {
            _gsmService = GetService<GSM>(); //Shouldn't be null here.
            _gsmService->WaitForConnection();
            _gsmClient = _gsmService->CreateClient();
            if (_gsmClient == nullptr)
            {
                LOGE(nameof(MQTT), "Failed to create client.");
                abort();
                return;
            }

            _mqtt.setClient(*_gsmClient);
            _mqtt.setServer(GetConfig(const char*, MQTT_BROKER), GetConfig(int, MQTT_PORT));
            _mqtt.setCallback(Callback);

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                _gsmService->QueueAction([this]()
                {
                    if (ValidateConnection())
                    {
                        _mqtt.loop();
                    }
                }, configIDLE_TASK_STACK_SIZE + 1024);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            _mqtt.disconnect();
            _gsmService->DestroyClient(_gsmClient);
            _gsmClient = nullptr;
            _gsmService = nullptr;
        }

    public:
        MQTT()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<GSM>();
            _subscribeTopic = GetConfig(String, MQTT_TOPIC) + "/" + GetConfig(String, MQTT_CLIENT_ID) + "/api";
            _publishTopic = GetConfig(String, MQTT_TOPIC) + "/" + GetConfig(String, MQTT_CLIENT_ID) + "/data";
        }

        const char* GetPublishTopic()
        {
            return _publishTopic.c_str();
        }

        bool WaitForConnection(TickType_t timeout = portMAX_DELAY)
        {
            return _connectedEvent.WaitOne(timeout);
        }

        bool Publish(const char* payload, uint32_t stackSize = configIDLE_TASK_STACK_SIZE, TickType_t timeout = portMAX_DELAY)
        {
            if (!WaitForConnection(timeout))
                return false;

            bool publishResult = false;
            bool gsmResult = _gsmService->QueueAction([this, &publishResult, payload]()
            {
                publishResult = _mqtt.publish(GetPublishTopic(), payload);
            }, stackSize, timeout);

            return gsmResult && publishResult;
        }
    };
};
