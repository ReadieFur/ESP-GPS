#pragma once

#include <Arduino.h>
#include <list>
#include <queue>
#include <algorithm>
#include "Scheduler.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "MQTT.hpp"
#include "HTTP.hpp"
#if defined(ESP8266)
#include "TimedLoop.hpp"
#endif

class SerialInterface
{
private:
    GPS* _gps;
    GSM* _gsm;
    MQTT* _mqtt = nullptr;
    HTTP* _http = nullptr;
    std::list<char> _serialBuffer;
    long _taskId = 0;
    #if defined(ESP8266)
    std::queue<String> _loopCommandQueue;
    #endif
    #ifdef DEBUG
    ushort _expectedResponses = 0;
    #endif

    static void SerialTask(void* args)
    {
        SerialInterface* self = static_cast<SerialInterface*>(args);

        #ifdef DEBUG
        auto gpsSerial = *self->_gps->DebugSerial;
        auto gsmSerial = *self->_gsm->DebugSerial;
        #endif

        while (Serial.available())
        {
            char c = Serial.read();
            Serial.write(c);
            self->_serialBuffer.push_back(c);
        }

        //Peek for newline character.
        while (std::find(self->_serialBuffer.begin(), self->_serialBuffer.end(), '\n') != self->_serialBuffer.end())
        {
            String str = "";
            do
            {
                char c = self->_serialBuffer.front();
                self->_serialBuffer.pop_front();
                str += c;

                if (c == '\n')
                    break;
            } while (!self->_serialBuffer.empty());
            

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
            #ifdef DEBUG
            else if (str.startsWith("gps toggle relay"))
            {
                Serial.print("Toggled GPS serial relay to: ");
                Serial.println(!self->_gps->DebugRelaySerial);
                self->_gps->DebugRelaySerial = !self->_gps->DebugRelaySerial;
            }
            else if (str.startsWith("gps location"))
            {
                Serial.println();
                Serial.print("Lat: ");
                Serial.println(self->_gps->TinyGps.location.lat(), 6);
                Serial.print("Lng: ");
                Serial.println(self->_gps->TinyGps.location.lng(), 6);
            }
            else if (str.startsWith("gsm connect"))
            {
                //Cant run from this timer callback on ESP8266.
                #if defined(ESP32)
                self->_gsm->Connect();
                #else
                self->_loopCommandQueue.push(str);
                #endif
            }
            #ifdef NET_MQTT
            else if (str.startsWith("mqtt connect"))
            {
                #if defined(ESP32)
                self->_mqtt->Connect();
                #else
                self->_loopCommandQueue.push(str);
                #endif
            }
            else if (str.startsWith("mqtt publish"))
            {
                #if defined(ESP32)
                self->_mqtt->Send(MQTT_TOPIC, str.c_str());
                #else
                self->_loopCommandQueue.push(str);
                #endif
            }
            #endif
            #ifdef NET_HTTP
            else if (str.startsWith("http test"))
            {
                #if defined(ESP32)
                HTTP::SRequest request =
                {
                    .method = HTTP_METHOD,
                    .path = HTTP_PATH,
                    .query = { { "millis", String(millis()).c_str() } }
                };
                self->_http->ProcessRequest(request);
                Serial.print("Request response code: ");
                Serial.println(request.responseCode);
                if (!request.responseBody.isEmpty())
                {
                    Serial.println("Response body:");
                    Serial.println(request.responseBody);
                }
                #else
                self->_loopCommandQueue.push(str);
                #endif
            }
            #endif
            #endif
            else
            {
                #ifdef DEBUG
                //Otherwise relay the input to the other serial busses.
                self->_expectedResponses++;

                // if (gpsSerial != nullptr)
                //     gpsSerial->print(str);

                if (self->_gsm != nullptr)
                    gsmSerial->print(str);
                #endif
            }
        }

        #ifdef DEBUG
        //Only read from the other serial busses if we have directly written anything to them. Otherwise leave them to be read by their owning classes.
        if (self->_expectedResponses == 0)
            return;

        if (gsmSerial != nullptr)
        {
            if (gpsSerial->available())
                self->_expectedResponses--;

            while (gsmSerial->available())
            {
                char c = gsmSerial->read();
                Serial.print(c);
            }
        }
        #endif
    }

public:
    SerialInterface(GPS* gps, GSM* gsm, MQTT* mqtt = nullptr, HTTP* http = nullptr)
    : _gps(gps), _gsm(gsm), _mqtt(mqtt), _http(http)
    {}

    void Begin()
    {
        if (_taskId != 0)
            return;
        Scheduler::Add(100, SerialTask);
    }

    void End()
    {
        if (_taskId == 0)
            return;
        Scheduler::Remove(_taskId);
    }

    #if defined(ESP8266)
    void Loop()
    {
        while (!_loopCommandQueue.empty())
        {
            String str = _loopCommandQueue.front();
            _loopCommandQueue.pop();
            if (false) {} //Placeholder.
            #ifdef DEBUG
            else if (str.startsWith("gsm connect"))
            {
                _gsm->Connect();
            }
            #ifdef NET_MQTT
            else if (str.startsWith("mqtt connect"))
            {
                _mqtt->Connect();
            }
            else if (str.startsWith("mqtt publish"))
            {
                bool mqttSendResult = _mqtt->Send(MQTT_TOPIC, (String("millis=") + String(millis())).c_str());
                Serial.println(String("MQTT publish result: ") + String(mqttSendResult));
            }
            #endif
            #ifdef NET_HTTP
            else if (str.startsWith("http test"))
            {
                HTTP::SRequest request =
                {
                    .method = HTTP_METHOD,
                    .path = HTTP_PATH,
                    .query = { { "millis", String(millis()) } }
                };
                _http->ProcessRequest(request);
                Serial.print("Request response code: ");
                Serial.println(request.responseCode);
                if (!request.responseBody.isEmpty())
                {
                    Serial.println("Response body:");
                    Serial.println(request.responseBody);
                }
            }
            #endif
            #endif
        }
    }
    #endif
};
