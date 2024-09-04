#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include "Config.h"
#include <SoftwareSerial.h>
#include "Scheduler.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"

GPS* gps;
GSM* gsm;
TinyGsmClient* mqttClient;
MQTT* mqtt;

void GpsTask(void* args)
{
    if (!gps->TinyGps.location.isUpdated())
        return;

    Serial.println();
    Serial.print("Lat: ");
    Serial.println(gps->TinyGps.location.lat(), 6);
    Serial.print("Lng: ");
    Serial.println(gps->TinyGps.location.lng(), 6);
}

void MqttCallback(char* topic, byte* message, unsigned int len)
{
}

void Main()
{
    //This has to be set first before any other objects are initalized as if they write to serial before this the baud is set to something different.
    Serial.begin(9600);

    #ifdef DEBUG
    Scheduler::Add(5000, [](void*){ Serial.println("SERIAL_ALIVE_CHECK:" + String(millis() / 1000)); }); //TODO: Testing only. Just for me to make sure the serial monitor or board hasn't frozen.
    #endif

    gps = new GPS(GPS_TX, GPS_RX);
    Scheduler::Add(1000, &GpsTask);

    gsm = new GSM(MODEM_TX, MODEM_RX, MODEM_APN, MODEM_PIN, MODEM_USERNAME, MODEM_PASSWORD);
    gsm->Connect();
    #if defined(ESP32)
    gsm->ConfigureAutomaticConnectionCheck(0);
    #endif

    mqttClient = gsm->CreateClient();
    mqtt = new MQTT(*mqttClient, MQTT_DEVICE_ID, MQTT_BROKER, MQTT_TOPIC, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD);
    mqtt->mqttClient->setCallback(MqttCallback);

    #ifdef DEBUG
    //Late task (allows time for me to connect to the serial.)
    delay(5000);
    #endif
}

#ifdef ARDUINO
void setup()
{
    Main();
}

void loop()
{
#if defined(ESP32)
    // vPortYield();
    vTaskDelete(NULL);
#elif defined(ESP8266)
    // gsm->Loop();
    yield();
#endif
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
