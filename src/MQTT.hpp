#pragma once

//https://randomnerdtutorials.com/esp32-cloud-mqtt-broker-sim800l/

#include <Client.h>
#include <PubSubClient.h>
#include <Scheduler.hpp>

class MQTT
{
private:
    const char *_deviceId, *_topic, *_username, *_password;
    ulong _validateConnectionTaskId;

    bool Reconnect()
    {
        if (!mqttClient->connect(_deviceId, _username, _password))
        {
            Serial.println("Failed to connect to MQTT.");
            return false;
        }

        if (!mqttClient->subscribe(_topic))
        {
            Serial.println("Failed to subscribe to MQTT topic.");
            return false;
        }

        return mqttClient->connected();
    }

    void ValidateConnection()
    {
        if (!mqttClient->connected())
            Reconnect();
    }

    // static void Callback(char* topic, unsigned char* message, unsigned int len)
    // {
    // }

public:
    PubSubClient* mqttClient;

    MQTT(Client& client, const char* deviceId, const char* broker, const char* topic, uint16_t port = 1883, const char* username = "", const char* password = "", ulong connectionCheckInterval = 10000U)
    : _deviceId(deviceId), _topic(topic), _username(strlen(username) ? username : NULL), _password(strlen(password) ? password : NULL)
    {
        mqttClient = new PubSubClient(client);
        mqttClient->setServer(broker, port);
        //mqttClient->setCallback(Callback); //Set externally.
        
        if (!Reconnect())
        {
            Serial.println("Failed to initialize MQTT.");
            abort();
        }

        _validateConnectionTaskId = Scheduler::Add(connectionCheckInterval, [](void* args)
        {
            MQTT* self = static_cast<MQTT*>(args);
            if (!self->mqttClient->connected())
                self->Reconnect();
        }, this);
    }
};
