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
        TinyGsmClient* _gsmClient = nullptr;
        PubSubClient _mqtt;
        String _publishTopic;
        String _subscribeTopic;

        static void Callback(char* topic, byte* payload, uint len)
        {
            String message = String(reinterpret_cast<const char*>(payload), len);
            ESP_LOGV(nameof(MQTT), "Message arrived:\n%s", message.c_str());
            //TODO: Send to API.
        }

        bool Connect()
        {
            if (_mqtt.connected())
                return true;

            ESP_LOGW(nameof(MQTT), "Disconnected from MQTT server...");

            //Connect to MQTT broker.
            if (!_mqtt.connect(GetConfig(const char*, MQTT_CLIENT_ID),
                GetConfig(const char*, MQTT_USERNAME), GetConfig(const char*, MQTT_PASSWORD)))
            {
                ESP_LOGE(nameof(MQTT), "Failed to connect to MQTT server...");
                return false;
            }

            _mqtt.subscribe(_subscribeTopic.c_str());

            ESP_LOGI(nameof(MQTT), "MQTT reconnected.");
            return _mqtt.connected();
        }

    protected:
        void RunServiceImpl() override
        {
            //Shouldn't be null here.
            GSM* gsmService = GetService<GSM>();

            _gsmClient = gsmService->CreateClient();
            if (_gsmClient == nullptr);
            {
                ESP_LOGE(nameof(MQTT), "Failed to create client.");
                abort();
                return;
            }

            _mqtt.setClient(*_gsmClient);

            _subscribeTopic = GetConfig(String, MQTT_TOPIC) + "/" + GetConfig(String, MQTT_CLIENT_ID) + "/api";
            _publishTopic = GetConfig(String, MQTT_TOPIC) + "/" + GetConfig(String, MQTT_CLIENT_ID) + "/data";

            //MQTT broker setup.
            _mqtt.setServer(GetConfig(const char*, MQTT_BROKER), GetConfig(int, MQTT_PORT));
            _mqtt.setCallback(Callback);

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                Connect();
                _mqtt.loop();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            _mqtt.disconnect();

            gsmService->DestroyClient(_gsmClient);
        }

    public:
        MQTT()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<GSM>();
        }

        const char* GetPublishTopic()
        {
            return _publishTopic.c_str();
        }
    };
};
