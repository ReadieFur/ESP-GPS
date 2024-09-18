#pragma once

#include "Board.h"
#include "Config.h"
#include <PubSubClient.h>
#include <Client.h>
#include "GSM.hpp"

class MQTT
{
private:
    static uint32_t _lastReconnectAttempt;
    static const char* _subscribeTopic;

    static void Callback(char* topic, byte* payload, uint len)
    {
        SerialMon.print("Message arrived [");
        SerialMon.print(topic);
        SerialMon.print("]: ");
        SerialMon.write(payload, len);
        SerialMon.println();

        //TODO: API.
    }

    static bool Connect()
    {
        SerialMon.print("Connecting to ");
        SerialMon.print(MQTT_BROKER);

        //Connect to MQTT broker.
        bool status = Mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
        if (status == false)
        {
            SerialMon.println(" fail");
            return false;
        }
        SerialMon.println(" success");

        // Mqtt.publish(MQTT_TOPIC, "Connected");
        Mqtt.subscribe(_subscribeTopic);

        return Mqtt.connected();
    }

public:
    static PubSubClient Mqtt;
    static const char* PublishTopic;

    static void Init()
    {
        //MQTT broker setup.
        Mqtt.setServer(MQTT_BROKER, MQTT_PORT);
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
const char* MQTT::_subscribeTopic = MQTT_TOPIC "/" MQTT_CLIENT_ID "/api";
PubSubClient MQTT::Mqtt(GSM::Client);
const char* MQTT::PublishTopic = MQTT_TOPIC "/" MQTT_CLIENT_ID "/data";
