#pragma once

//https://randomnerdtutorials.com/esp32-cloud-mqtt-broker-sim800l/

#include <Client.h>
#include <PubSubClient.h>
#include <Scheduler.hpp>
#include <list>
#include <algorithm>
#include <map>

class MQTT
{
private:
    typedef void(*TCallback)(MQTT* sender, const char* topic, const char* message);

    const char *_deviceId, *_username, *_password;
    std::map<const char*, std::list<TCallback>> _subscriptions;
    #if defined(ESP32)
    ulong _validateConnectionTaskId;
    ulong _automaticCallbackDispatcherTaskId;
    #endif

    //Unfortunately for now I will have to broadcast this topic to all instances of this class that match the topic as the underlying PubSubClient library does not have a way to determine the instance the message was published from, nor does it allow for any arbitrary parameters being passed as arguments to the callback function.
    void ProcessCallback(char* topic, uint8_t* message, unsigned int len)
    {
        auto callbacks = _subscriptions.find(topic);
        if (callbacks == _subscriptions.end())
            return;

        char* msg = new char[len + 1];
        if (msg == nullptr)
        {
            Serial.println("Dropping MQTT message: Failed to allocate message buffer.");
            return;
        }
        memcpy(msg, message, len);
        msg[len] = '\0';

        for (auto &&callback : callbacks->second)
            callback(this, topic, msg);

        delete[] msg;
    }

public:
    PubSubClient mqttClient;

    MQTT(Client& client, const char* deviceId, const char* broker, uint16_t port = 1883, const char* username = "", const char* password = "")
    : _deviceId(deviceId), _username(strlen(username) ? username : NULL), _password(strlen(password) ? password : NULL)
    {
        mqttClient.setClient(client);
        mqttClient.setServer(broker, port);
        mqttClient.setCallback([this](char* topic, uint8_t* message, unsigned int len){ this->ProcessCallback(topic, message, len); });
    }

    ~MQTT()
    {
        #if defined(ESP32)
        Scheduler::Remove(_validateConnectionTaskId);
        Scheduler::Remove(_automaticCallbackDispatcherTaskId);
        #endif
    }

    #if defined(ESP32)
    /// @brief Enable task based automatic connection checking.
    /// @param connectionCheckInterval The time between checks of the connection. Set to 0 to disable automatic checks.
    /// @note Only available on ESP32.
    void ConfigureAutomaticConnectionCheck(ulong connectionCheckInterval)
    {
        if (connectionCheckInterval == 0)
        {
            Scheduler::Remove(_validateConnectionTaskId);
            _validateConnectionTaskId = 0;
            return;
        }

        if (_validateConnectionTaskId != 0)
            Scheduler::Remove(_validateConnectionTaskId);

        //Has to be limited to the ESP32 because the client could be a GSM client, which on the ESP8266 can cause stack overflow errors when dispatched from a timer.
        _validateConnectionTaskId = Scheduler::Add(connectionCheckInterval, [](void* args) { static_cast<MQTT*>(args)->self->Connect(); }, this);
    }

    /// @brief Enable task based automatic subscription polling to the server.
    /// @param serverPollInterval The time between polling the server for new messages. Set to 0 to disable automatic checks.
    /// @note Only available on ESP32.
    void ConfigureAutomaticCallbackDispatcher(ulong serverPollInterval)
    {
        if (serverPollInterval == 0)
        {
            Scheduler::Remove(_automaticCallbackDispatcherTaskId);
            _automaticCallbackDispatcherTaskId = 0;
            return;
        }

        if (_automaticCallbackDispatcherTaskId != 0)
            Scheduler::Remove(_automaticCallbackDispatcherTaskId);
        
        _automaticCallbackDispatcherTaskId = Scheduler::Add(serverPollInterval, [](void* args) { static_cast<MQTT*>(args)->mqttClient.loop(); }, this);
    }
    #endif

    int Connect()
    {
        if (mqttClient.connected())
        {
            #ifdef DEBUG
            Serial.println("MQTT alive.");
            #endif
            return true;
        }

        if (!mqttClient.connect(_deviceId, _username, _password))
        {
            int state = mqttClient.state();
            Serial.print("Failed to connect to MQTT: ");
            Serial.println(state);
            return state;
        }

        for (auto &&topic : _subscriptions)
        {
            if (!mqttClient.subscribe(topic.first))
            {
                Serial.print("Failed to subscribe to MQTT topic: ");
                Serial.println(topic.first);
            }
        }

        return mqttClient.connected() ? MQTT_CONNECTED : mqttClient.state();
    }

    bool Subscribe(const char* topic)
    {
        #if false
        if (_subscriptions.find(topic) != _subscriptions.end())
        {
            //Topic already exists.
            return true;
        }

        if (!mqttClient.subscribe(topic))
        {
            Serial.println(String("Failed to subscribe to MQTT topic: ") + topic);
            return false;
        }

        //This creates a new entry when written to if the key does not exist.
        _subscriptions[topic];
        return true;
        #else
        //For now have this setup so that callbacks and subscriptions have to be setup separately.
        if (!mqttClient.subscribe(topic))
        {
            Serial.println(String("Failed to subscribe to MQTT topic: ") + topic);
            return false;
        }
        return true;
        #endif
    }

    void Unsubscribe(const char* topic)
    {
        mqttClient.unsubscribe(topic);
        // _subscriptions.remove(topic);
    }

    void AddCallback(const char* topic, TCallback callback)
    {
        //This creates a new entry when written to if the key does not exist.
        auto callbacks = _subscriptions[topic];

        //If the callback already exists in the list for this topic, ignore it and return.
        if (std::find(callbacks.begin(), callbacks.end(), callback) != callbacks.end())
            return;

        callbacks.push_back(callback);
    }

    void RemoveCallback(const char* topic, TCallback callback)
    {
        auto callbacks = _subscriptions.find(topic);
        if (callbacks == _subscriptions.end())
            return;

        //This does nothing if the callback doesn't exist in the list.
        callbacks->second.remove(callback);

        if (callbacks->second.empty())
            _subscriptions.erase(topic);
    }

    bool Send(const char* topic, const char* message)
    {
        return mqttClient.publish(topic, message);
    }

    #if defined(ESP8266)
    //Automatic on ESP32.
    void Loop()
    {
        mqttClient.loop();
    }
    #endif
};
