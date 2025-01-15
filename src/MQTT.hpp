#pragma once

#include "Service/AService.hpp"
#include <PubSubClient.h>
#include <Wstring.h>
#include "Storage.hpp"
#include "Logging.hpp"
#include "Helpers.h"
#include "GSM.hpp"
#include <TinyGsmClient.h>
#include <vector>
#include <esp_random.h>

namespace ReadieFur::EspGps
{
    //TODO: Use the MQTT service provided by the GSM module if supported (probably more efficient for power and data usage).
    class MQTT : public Service::AService
    {
    private:
        GSM* _gsmService = nullptr;
        #if TINY_GSM_MQTT_CLI_COUNT > 0
        static std::vector<int> _clients;
        TinyGsm* _modem = nullptr;
        int _clientIndex = 0;
        #else
        TinyGsmClient* _gsmClient = nullptr;
        PubSubClient _mqtt;
        #endif
        String _publishTopic;
        String _subscribeTopic;
        Event::ManualResetEvent _connectedEvent;

        static void Callback(const char* topic, const byte* payload, size_t length)
        {
            String message = String(reinterpret_cast<const char*>(payload), length);
            LOGV(nameof(MQTT), "Message arrived:\n%s", message.c_str());
            //TODO: Send to API.
        }

        #if TINY_GSM_MQTT_CLI_COUNT > 0
        static void CallbackWrapper(const char* topic, const uint8_t* payload, uint32_t len)
        {
            Callback(topic, payload, len);
        }
        #else
        static void CallbackWrapper(char* topic, byte* payload, uint len)
        {
            Callback(topic, payload, len);
        }
        #endif

        bool ValidateConnection()
        {
            #if TINY_GSM_MQTT_CLI_COUNT > 0
            if (_modem->mqtt_connected(_clientIndex))
            //No need to check if GSM is connected as this callback will only be run if it is connected.
            #else
            if (_mqtt.connected())
            #endif
            {
                return true;
            }

            if (_connectedEvent.IsSet())
            {
                LOGW(nameof(MQTT), "Disconnected from MQTT server...");
                _connectedEvent.Clear();
            }

            //Connect to MQTT broker.
            #if TINY_GSM_MQTT_CLI_COUNT > 0
            if (!_modem->mqtt_connect(_clientIndex,
                GetConfig(const char*, MQTT_BROKER),
                GetConfig(int, MQTT_PORT),
                GetConfig(const char*, MQTT_CLIENT_ID),
                GetConfig(const char*, MQTT_USERNAME),
                GetConfig(const char*, MQTT_PASSWORD)))
            #else
            if (!_mqtt.connect(GetConfig(const char*, MQTT_CLIENT_ID),
                GetConfig(const char*, MQTT_USERNAME), GetConfig(const char*, MQTT_PASSWORD)))
            #endif
            {
                LOGE(nameof(MQTT), "Failed to connect to MQTT server...");
                return false;
            }

            #if TINY_GSM_MQTT_CLI_COUNT > 0
            _modem->mqtt_subscribe(_clientIndex, _subscribeTopic.c_str());
            #else
            _mqtt.subscribe(_subscribeTopic.c_str());
            #endif

            LOGI(nameof(MQTT), "MQTT reconnected.");
            _connectedEvent.Set();
            return true;
        }

        void FullRelease()
        {
            #if TINY_GSM_MQTT_CLI_COUNT > 0
            _gsmService->QueueAction([this]()
            {
                #if true
                for (int i = 0; i < TINY_GSM_MQTT_CLI_COUNT; i++)
                    _modem->mqtt_disconnect(i); //This call handles all three required commands in the required order: DISCONNECT, RELEASE, STOP.
                #else
                //Fast release:
                for (int i = 0; i < TINY_GSM_MQTT_CLI_COUNT; i++)
                {
                    _modem->sendAT("+CMQTTDISC=", i, ",0");
                    _modem->waitResponse(3000);
                    _modem->waitResponse(10000UL, "+CMQTTDISC: ");
                    _modem->sendAT("+CMQTTREL=", i);
                    _modem->waitResponse(3000);
                }
                _modem->sendAT("+CMQTTSTOP");
                _modem->waitResponse("+CMQTTSTOP: ");
                _modem->waitResponse(3000);
                #endif
            }, ServiceEntrypointStackDepth);
            #else
            _mqtt.disconnect();
            _gsmService->DestroyClient(_gsmClient);
            #endif
        }

    protected:
        void RunServiceImpl() override
        {
            _gsmService = GetService<GSM>(); //Shouldn't be null here.
            _gsmService->WaitForConnection();

            #if TINY_GSM_MQTT_CLI_COUNT > 0
            _modem = _gsmService->GetModem();

            #if true
            switch (esp_reset_reason())
            {
            case ESP_RST_UNKNOWN:
            case ESP_RST_DEEPSLEEP:
            case ESP_RST_POWERON:
                FullRelease(); //Clear any old connections as this code will fail of there are any, the downside to this is that it makes the boot much slower.
                break;
            default:
                break;
            }
            #else
            //See GSM ModemInit as for why I am skipping this right now.
            #endif

            _gsmService->QueueAction([this]()
            {
                if (!_modem->mqtt_begin(false)) //SSL currently not implemented.
                {
                    LOGE(nameof(MQTT), "Failed to start MQTT client.");
                    abort();
                    return;
                }

                _modem->mqtt_set_callback(CallbackWrapper);

                //Get first free mqtt instance ID.
                _clientIndex = 0;
                for (int i = 0; i < TINY_GSM_MQTT_CLI_COUNT; i++)
                {
                    if (std::find(_clients.begin(), _clients.end(), i) == _clients.end())
                    {
                        _clientIndex = i;
                        break;
                    }

                    if (i == TINY_GSM_MQTT_CLI_COUNT - 1)
                    {
                        LOGW(nameof(MQTT), "Maximum MQTT clients reached.");
                        abort();
                        return;
                    }
                }
            }, ServiceEntrypointStackDepth);
            #else
            _gsmClient = _gsmService->CreateClient();
            if (_gsmClient == nullptr)
            {
                LOGE(nameof(MQTT), "Failed to create client.");
                abort();
                return;
            }

            _mqtt.setClient(*_gsmClient);
            _mqtt.setServer(GetConfig(const char*, MQTT_BROKER), GetConfig(int, MQTT_PORT));
            _mqtt.setCallback(CallbackWrapper);
            #endif

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                _gsmService->QueueAction([this]()
                {
                    if (!ValidateConnection())
                        return;

                    #if TINY_GSM_MQTT_CLI_COUNT > 0
                    _modem->mqtt_handle();
                    #else
                    _mqtt.loop();
                    #endif
                }, configIDLE_TASK_STACK_SIZE + 1024);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            FullRelease();
            #if TINY_GSM_MQTT_CLI_COUNT > 0
            _modem = nullptr;
            #else
            _gsmClient = nullptr;
            #endif
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
                #if TINY_GSM_MQTT_CLI_COUNT > 0
                publishResult = _modem->mqtt_publish(_clientIndex, GetPublishTopic(), payload);
                #else
                publishResult = _mqtt.publish(GetPublishTopic(), payload);
                #endif
            }, stackSize, timeout);
            return gsmResult && publishResult;
        }
    };
};

std::vector<int> ReadieFur::EspGps::MQTT::_clients;
