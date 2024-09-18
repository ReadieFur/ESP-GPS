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

    static void Callback(char* topic, byte* payload, uint len)
    {
        SerialMon.print("Message arrived [");
        SerialMon.print(topic);
        SerialMon.print("]: ");
        SerialMon.write(payload, len);
        SerialMon.println();
    }

    static bool Connect()
    {
        SerialMon.print("Connecting to ");
        SerialMon.print(MQTT_BROKER);

        //Connect to MQTT broker.
        bool status = mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
        if (status == false)
        {
            SerialMon.println(" fail");
            return false;
        }
        SerialMon.println(" success");

        mqtt.publish(MQTT_TOPIC, "GsmClientTest started");
        mqtt.subscribe(MQTT_TOPIC);

        return mqtt.connected();
    }

public:
    static PubSubClient mqtt;

    static void Init()
    {
        //MQTT broker setup.
        mqtt.setServer(MQTT_BROKER, MQTT_PORT);
        mqtt.setCallback(Callback);
    }

    static bool Loop()
    {
        if (!mqtt.connected())
        {
            DBG("MQTT not connected.");
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

        mqtt.loop();

        return true;
    }
};

PubSubClient MQTT::mqtt(GSM::Client);
uint32_t MQTT::_lastReconnectAttempt = 0;
