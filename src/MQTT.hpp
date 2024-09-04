#pragma once

//https://randomnerdtutorials.com/esp32-cloud-mqtt-broker-sim800l/

#include <Client.h>
#include <PubSubClient.h>
#include <Scheduler.hpp>
#include <list>
#include <algorithm>
#include <map>
#include <tuple>

class MQTT
{
private:
    typedef void(*TCallback)(/*MQTT* sender, */const char* topic, const char* message);

    static std::map<const char*, std::list<std::tuple<MQTT*, TCallback>>> _subscriptionCallbacks;

    const char *_deviceId, *_username, *_password;
    #if defined(ESP32)
    ulong _validateConnectionTaskId;
    ulong _automaticCallbackDispatcherTaskId;
    #endif
    std::list<const char*> _subscriptions;

    void ValidateConnection()
    {
        if (!mqttClient->connected())
            Connect();
    }

    //Unfortunately for now I will have to broadcast this topic to all instances of this class that match the topic as the underlying PubSubClient library does not have a way to determine the instance the message was published from, nor does it allow for any arbitrary parameters being passed as arguments to the callback function.
    static void Callback(char* topic, uint8_t* message, unsigned int len)
    {
        auto callbacks = _subscriptionCallbacks.find(topic);
        if (callbacks == _subscriptionCallbacks.end())
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
            std::get<1>(callback)(topic, msg);

        delete[] msg;
    }

public:
    PubSubClient* mqttClient;

    MQTT(Client& client, const char* deviceId, const char* broker, uint16_t port = 1883, const char* username = "", const char* password = "")
    : _deviceId(deviceId), _username(strlen(username) ? username : NULL), _password(strlen(password) ? password : NULL)
    {
        mqttClient = new PubSubClient(client);
        mqttClient->setServer(broker, port);
        mqttClient->setCallback(Callback);
    }

    ~MQTT()
    {
        #if defined(ESP32)
        Scheduler::Remove(_validateConnectionTaskId);
        Scheduler::Remove(_automaticCallbackDispatcherTaskId);
        #endif

        //Remove all callback subscriptions for this instance.
        for (auto it = _subscriptionCallbacks.begin(); it != _subscriptionCallbacks.end(); /*No increment here*/)
        {
            std::list<std::tuple<MQTT*, TCallback>>& callbackList = it->second;

            //Remove tuples where MQTT* matches the given pointer.
            callbackList.remove_if([this](const std::tuple<MQTT*, TCallback>& item) { return std::get<0>(item) == this; });

            //If the list is empty, erase the key from the map.
            if (callbackList.empty())
                it = _subscriptionCallbacks.erase(it);
            else
                ++it; //Move to the next item.
        }

        delete mqttClient;
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
        _validateConnectionTaskId = Scheduler::Add(connectionCheckInterval, [](void* args)
        {
            MQTT* self = static_cast<MQTT*>(args);
            if (!self->mqttClient->connected())
                self->Reconnect();
        }, this);
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
        
        _automaticCallbackDispatcherTaskId = Scheduler::Add(serverPollInterval, [](void* args)
            { static_cast<MQTT*>(args)->mqttClient->loop(); }, this);
    }
    #endif

    int Connect()
    {
        if (!mqttClient->connect(_deviceId, _username, _password))
        {
            Serial.println("Failed to connect to MQTT.");
            return mqttClient->state();
        }

        for (auto &&topic : _subscriptions)
            if (!mqttClient->subscribe(topic))
                Serial.println(String("Failed to subscribe to MQTT topic: ") + topic);

        return mqttClient->connected() ? MQTT_CONNECTED : mqttClient->state();
    }

    bool Subscribe(const char* topic)
    {
        if (std::find(_subscriptions.begin(), _subscriptions.end(), topic) != _subscriptions.end())
        {
            //Topic already exists.
            return true;
        }

        if (!mqttClient->subscribe(topic))
        {
            Serial.println(String("Failed to subscribe to MQTT topic: ") + topic);
            return false;
        }

        _subscriptions.push_back(topic);
        return true;
    }

    void Unsubscribe(const char* topic)
    {
        mqttClient->unsubscribe(topic);
        _subscriptions.remove(topic);
    }

    void AddCallback(const char* topic, TCallback callback)
    {
        //This creates a new entry when written to if the key does not exist.
        auto callbacks = _subscriptionCallbacks[topic];

        //If the callback already exists in the list for this topic, ignore it and return.
        if (std::find(_subscriptionCallbacks.begin(), _subscriptionCallbacks.end(), callback) != _subscriptionCallbacks.end())
            return;

        callbacks.push_back({this, callback});
    }

    void RemoveCallback(const char* topic, TCallback callback)
    {
        auto callbacks = _subscriptionCallbacks.find(topic);
        if (callbacks == _subscriptionCallbacks.end())
            return;

        callbacks->second.remove_if([callback](auto entry)
        {
            //Only remove matching callbacks where the sender is this.
            return std::get<0>(entry) == this
                && std::get<1>(entry) == callback;
        });

        if (callbacks->second.empty())
            _subscriptionCallbacks.erase(topic);
    }

    #if defined(ESP8266)
    //Automatic on ESP32.
    void Loop()
    {
        mqttClient->loop();
    }
    #endif
};
