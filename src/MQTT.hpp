#pragma once

#include "Service/AService.hpp"
#include <PubSubClient.h>
#include <Wstring.h>
#include "Storage.hpp"
#include <esp_log.h>
#include "Helpers.h"
#include "GSM.hpp"
#include <TinyGsmClient.h>

namespace ReadieFur::EspGps
{
    class MQTT : public Service::AService
    {
    private:
        GSM* _gsmService = nullptr;
        TinyGsmClient* _gsmClient = nullptr;
        PubSubClient _mqtt;
        String _publishTopic;
        String _subscribeTopic;
        bool _wasConnected = false;

        static void Callback(char* topic, byte* payload, uint len)
        {
            String message = String(reinterpret_cast<const char*>(payload), len);
            ESP_LOGV(nameof(MQTT), "Message arrived:\n%s", message.c_str());
            //TODO: Send to API.
        }

        bool ValidateConnection()
        {
            bool gsmConnected = _gsmService->IsConnected();
            if (gsmConnected && _mqtt.connected())
                return true;

            if (_wasConnected)
            {
                ESP_LOGW(nameof(MQTT), "Disconnected from MQTT server...");
                _wasConnected = false;
            }

            if (!gsmConnected)
            {
                //Skip this attempt and try again when the connection is restored.
                return false;
            }

            //Connect to MQTT broker.
            if (!_mqtt.connect(GetConfig(const char*, MQTT_CLIENT_ID),
                GetConfig(const char*, MQTT_USERNAME), GetConfig(const char*, MQTT_PASSWORD)))
            {
                ESP_LOGE(nameof(MQTT), "Failed to connect to MQTT server...");
                return false;
            }

            _mqtt.subscribe(_subscribeTopic.c_str());

            ESP_LOGI(nameof(MQTT), "MQTT reconnected.");
            return _wasConnected = true;
        }

    protected:
        void RunServiceImpl() override
        {
            //Shouldn't be null here.
            _gsmService = GetService<GSM>();
            _gsmService->WaitForConnection();
            _gsmClient = _gsmService->CreateClient();
            if (_gsmClient == nullptr)
            {
                ESP_LOGE(nameof(MQTT), "Failed to create client.");
                abort();
                return;
            }

            _mqtt.setClient(*_gsmClient);
            _mqtt.setServer(GetConfig(const char*, MQTT_BROKER), GetConfig(int, MQTT_PORT));
            _mqtt.setCallback(Callback);

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                if (ValidateConnection())
                    _mqtt.loop();
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
    };
};
