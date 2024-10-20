#pragma once

#include "Board.h"
#include "Config.h"
#include <PubSubClient.h>
#include <Client.h>
#include "GSM.hpp"
#include "API.hpp"
#include <Wstring.h>

class MQTT
{
private:
    static uint32_t _lastReconnectAttempt;
    static String _subscribeTopic;

    static void Callback(char* topic, byte* payload, uint len)
    {
        SerialMon.print("Message arrived [");
        SerialMon.print(topic);
        SerialMon.print("]: ");
        SerialMon.write(payload, len);
        SerialMon.println();

        API::ProcessMessage(String(reinterpret_cast<const char*>(payload), len));

        // //Reading the source of the MQTT library the payload pointer is never freed so I will do that here manually.
        // free(payload);
        //Ignore the above, the library stores everything in a complex memory structure that it does manage.
    }

    static bool Connect()
    {
        SerialMon.print("Connecting to ");
        SerialMon.print(GetConfig(const char*, MQTT_BROKER));

        //Connect to MQTT broker.
        bool status = Mqtt.connect(GetConfig(const char*, MQTT_CLIENT_ID), GetConfig(const char*, MQTT_USERNAME), GetConfig(const char*, MQTT_PASSWORD));
        if (status == false)
        {
            SerialMon.println(" fail");
            return false;
        }
        SerialMon.println(" success");

        // Mqtt.publish(MQTT_TOPIC, "Connected");
        Mqtt.subscribe(_subscribeTopic.c_str());

        return Mqtt.connected();
    }

public:
    static PubSubClient Mqtt;
    static String PublishTopic;

    static void Init()
    {
        _subscribeTopic = GetConfig(String, MQTT_TOPIC) + "/" + GetConfig(String, MQTT_CLIENT_ID) + "/api";
        PublishTopic = GetConfig(String, MQTT_TOPIC) + "/" + GetConfig(String, MQTT_CLIENT_ID) + "/data";

        //MQTT broker setup.
        Mqtt.setServer(GetConfig(const char*, MQTT_BROKER), GetConfig(int, MQTT_PORT));
        Mqtt.setCallback(Callback);
    }

    static bool Loop()
    {
        if (!Mqtt.connected())
        {
            SerialMon.println("MQTT not connected.");
            // SerialMon.println("MQTT not connected.");
            //Reconnect every x milliseconds.
            uint32_t now = millis();
            if (now - _lastReconnectAttempt > 10000L)
            {
                _lastReconnectAttempt = now;
                if (Connect())
                    _lastReconnectAttempt = 0;
            }
            delay(100);
            return false;
        }

        Mqtt.loop();

        return true;
    }
};

uint32_t MQTT::_lastReconnectAttempt = 0;
String MQTT::_subscribeTopic;
PubSubClient MQTT::Mqtt(GSM::Client);
String MQTT::PublishTopic;
