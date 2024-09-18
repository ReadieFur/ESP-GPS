#pragma once

#include "Board.h"
#include "Config.h"
#include <PubSubClient.h>
#include <Client.h>
#include "GSM.hpp"

class MQTT
{
private:
    static PubSubClient mqtt;
    static uint32_t lastReconnectAttempt;

    static void mqttCallback(char *topic, byte *payload, unsigned int len)
    {
        SerialMon.print("Message arrived [");
        SerialMon.print(topic);
        SerialMon.print("]: ");
        SerialMon.write(payload, len);
        SerialMon.println();
    }

    static boolean mqttConnect()
    {
        SerialMon.print("Connecting to ");
        SerialMon.print(MQTT_BROKER);

        // Connect to MQTT Broker
        boolean status = mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);

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
    static void Init()
    {
        // MQTT Broker setup
        mqtt.setServer(MQTT_BROKER, MQTT_PORT);
        mqtt.setCallback(mqttCallback);
    }

    static void Loop()
    {
        if (!mqtt.connected())
        {
            SerialMon.println("=== MQTT NOT CONNECTED ===");
            //Reconnect every 10 seconds.
            uint32_t t = millis();
            if (t - lastReconnectAttempt > 10000L)
            {
                lastReconnectAttempt = t;
                if (mqttConnect())
                    lastReconnectAttempt = 0;
            }
            delay(100);
            return;
        }

        mqtt.loop();
    }
};

PubSubClient MQTT::mqtt(GSM::client);
uint32_t MQTT::lastReconnectAttempt = 0;
