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

GPS* gps;
GSM* gsm;

#ifdef NET_MQTT
#include "MQTT.hpp"
TinyGsmClient* mqttClient;
MQTT* mqtt;
TimedLoop mqttLoop(100);
#endif

#ifdef NET_HTTP
#include "HTTP.hpp"
TinyGsmClient* httpClient;
HTTP* http;
#endif

#if defined(ESP8266)
TimedLoop connectionCheckLoop(1000);
#endif

#ifdef DEBUG
#include <list>
#include <queue>
#include <algorithm>
ushort expectedResponsesForSerial = 0;
std::list<char> serialBuffer;
std::queue<String> serialLoopCommandQueue;
void SerialTask(void* args)
{
    auto gpsSerial = *gps->DebugSerial;
    auto gsmSerial = *gsm->DebugSerial;

    while (Serial.available())
    {
        char c = Serial.read();
        Serial.write(c);
        serialBuffer.push_back(c);
    }

    //Peek for newline character.
    while (std::find(serialBuffer.begin(), serialBuffer.end(), '\n') != serialBuffer.end())
    {
        String str = "";
        do
        {
            char c = serialBuffer.front();
            serialBuffer.pop_front();
            str += c;

            if (c == '\n')
                break;
        } while (!serialBuffer.empty());
        

        //Ignore empty strings.
        bool containsRealCharacter = false;
        for (auto &&c : str)
        {
            containsRealCharacter = isalnum(c);
            if (containsRealCharacter)
                break;
        }
        if (!containsRealCharacter)
            continue;

        //Check if serial command is to be intercepted.
        if (str.startsWith("ping"))
        {
            Serial.println("pong");
        }
        else if (str.startsWith("reset"))
        {
            ESP.restart();
        }
        else if (str.startsWith("gps toggle relay"))
        {
            Serial.print("Toggled GPS serial relay to: ");
            Serial.println(!gps->DebugRelaySerial);
            gps->DebugRelaySerial = !gps->DebugRelaySerial;
        }
        else if (str.startsWith("gps location"))
        {
            Serial.println();
            Serial.print("Lat: ");
            Serial.println(gps->TinyGps.location.lat(), 6);
            Serial.print("Lng: ");
            Serial.println(gps->TinyGps.location.lng(), 6);
        }
        else if (str.startsWith("gsm connect"))
        {
            //Cant run from this timer callback on ESP8266.
            #if defined(ESP32)
            gsm->Connect();
            #else
            serialLoopCommandQueue.push(str);
            #endif
        }
        #ifdef NET_MQTT
        else if (str.startsWith("mqtt connect"))
        {
            #if defined(ESP32)
            mqtt->Connect();
            #else
            serialLoopCommandQueue.push(str);
            #endif
        }
        else if (str.startsWith("mqtt publish"))
        {
            #if defined(ESP32)
            mqtt->Send(MQTT_TOPIC, str.c_str());
            #else
            serialLoopCommandQueue.push(str);
            #endif
        }
        #endif
        #ifdef NET_HTTP
        else if (str.startsWith("http"))
        {
            #if defined(ESP32)
            HTTP::SRequest request =
            {
                .method = HTTP_METHOD,
                .path = HTTP_PATH,
                .query = { { "millis", String(millis()).c_str() } }
            };
            http->ProcessRequest(request);
            Serial.print("Request response code: ");
            Serial.println(request.responseCode);
            if (!request.responseBody.isEmpty())
            {
                Serial.println("Response body:");
                Serial.println(request.responseBody);
            }
            #else
            serialLoopCommandQueue.push(str);
            #endif
        }
        #endif
        else
        {
            //Otherwise relay the input to the other serial busses.
            expectedResponsesForSerial++;

            // if (gpsSerial != nullptr)
            //     gpsSerial->print(str);

            if (gsm != nullptr)
                gsmSerial->print(str);
        }
    }

    //Only read from the other serial busses if we have directly written anything to them. Otherwise leave them to be read by their owning classes.
    if (expectedResponsesForSerial == 0)
        return;

    //Shouldn't be nullptr by the time I get to interact with the terminal but added for safety.
    // if (gpsSerial != nullptr)
    // {
    //     if (gpsSerial->available())
    //         expectedResponsesForSerial--;

    //     while (gpsSerial->available())
    //     {
    //         char c = gpsSerial->read();
    //         Serial.write(c);
    //     }
    // }

    if (gsmSerial != nullptr)
    {
        if (gpsSerial->available())
            expectedResponsesForSerial--;

        while (gsmSerial->available())
        {
            char c = gsmSerial->read();
            Serial.print(c);
        }
    }
}
#endif

#if defined(DEBUG) && defined(NET_MQTT)
void MqttCallback(MQTT* sender, const char* topic, const char* message)
{
    Serial.println(String("MQTT message received at topic '") + topic + "': " + message);
}
#endif

void Main()
{
    #ifdef DEBUG
    //Allows time for me to connect to the serial.
    delay(2000);
    #endif

    //This has to be set first before any other objects are initalized as if they write to serial before this the baud is set to something different.
    Serial.begin(9600);

    #ifdef DEBUG
    // Scheduler::Add(5000, [](void*){ Serial.println("SERIAL_ALIVE_CHECK:" + String(millis() / 1000 - 2)); }); //TODO: Testing only. Just for me to make sure the serial monitor or board hasn't frozen. (Not needed now I have ping/pong).
    Scheduler::Add(100, SerialTask);
    #endif

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
    #ifdef DEBUG
    mqtt->AddCallback(MQTT_TOPIC, MqttCallback);
    #endif
    #endif

    #ifdef NET_HTTP
    httpClient = gsm->CreateClient();
    http = new HTTP(*httpClient, HTTP_ADDRESS, HTTP_PORT);
    #endif

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
    // connectionCheckLoop.Loop();
    // mqttLoop.Loop();

    #ifdef DEBUG
    while (!serialLoopCommandQueue.empty())
    {
        String str = serialLoopCommandQueue.front();
        serialLoopCommandQueue.pop();
        if (str.startsWith("gsm connect"))
            gsm->Connect();
        #ifdef NET_MQTT
        else if (str.startsWith("mqtt connect"))
            mqtt->Connect();
        else if (str.startsWith("mqtt publish"))
            mqtt->Send(MQTT_TOPIC, str.c_str());
        #endif
        #ifdef NET_HTTP
        else if (str.startsWith("http"))
        {
            HTTP::SRequest request =
            {
                .method = HTTP_METHOD,
                .path = HTTP_PATH,
                .query = { { "millis", String(millis()) } }
            };
            http->ProcessRequest(request);
            Serial.print("Request response code: ");
            Serial.println(request.responseCode);
            if (!request.responseBody.isEmpty())
            {
                Serial.println("Response body:");
                Serial.println(request.responseBody);
            }
        }
        #endif
    }
    #endif

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
