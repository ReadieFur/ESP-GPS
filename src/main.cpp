#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(ESP8266)
#include "TimedLoop.hpp"
#endif
#include "Config.h"
#include <SoftwareSerial.h>
#include "Scheduler.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "SerialInterface.hpp"

GPS* gps;
GSM* gsm;

#ifdef NET_MQTT
#include "MQTT.hpp"
TinyGsmClient* mqttClient;
MQTT* mqtt;
#if defined(ESP8266)
TimedLoop mqttLoop(100);
#endif
#endif

#ifdef NET_HTTP
#include "HTTP.hpp"
TinyGsmClient* httpClient;
HTTP* http;
#endif

#if defined(ESP8266)
TimedLoop connectionCheckLoop(1000);
#endif

SerialInterface* serialInterface;

void Main()
{
    #ifdef DEBUG
    //Allows time for me to connect to the serial.
    delay(2000);
    #endif

    //This has to be set first before any other objects are initalized as if they write to serial before this the baud is set to something different.
    Serial.begin(9600);

    gps = new GPS(GPS_TX, GPS_RX);

    gsm = new GSM(MODEM_TX, MODEM_RX, MODEM_APN, MODEM_PIN, MODEM_USERNAME, MODEM_PASSWORD);
    gsm->Connect();
    #if defined(ESP32)
    gsm->ConfigureAutomaticConnectionCheck(0);
    #endif

    #ifdef NET_MQTT
    mqttClient = gsm->CreateClient();
    mqtt = new MQTT(*mqttClient, MQTT_DEVICE_ID, MQTT_BROKER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD);
    mqtt->Subscribe(MQTT_TOPIC);
    #endif

    #ifdef NET_HTTP
    httpClient = gsm->CreateClient();
    http = new HTTP(*httpClient, HTTP_ADDRESS, HTTP_PORT);
    #endif

    serialInterface = new SerialInterface(
        gps,
        gsm,
        #ifdef NET_MQTT
        mqtt,
        #else
        nullptr,
        #endif
        #ifdef NET_HTTP
        http
        #else
        nullptr
        #endif
    );
    serialInterface->Begin();

    #if defined(ESP8266)
    connectionCheckLoop.Callback = []()
    {
        gsm->Connect();
        #ifdef NET_MQTT
        mqtt->Connect();
        #endif
    };

    #ifdef NET_MQTT
    mqttLoop.Callback = [](){ mqtt->Loop(); };
    #endif
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
    connectionCheckLoop.Loop();
    #ifdef NET_MQTT
    mqttLoop.Loop();
    #endif
    serialInterface->Loop();
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
