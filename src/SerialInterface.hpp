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

class SerialInterface
{
private:
    GPS* _gps;
    GSM* _gsm;
    MQTT* _mqtt; //Not nullptr when enabled in the config.
    HTTP* _http; //Not nullptr when enabled in the config.
    std::list<char> _serialBuffer;
    long _serialTaskId = 0;
    #if defined(ESP32)
    long _processTaskId = 0;
    #endif
    #ifdef DEBUG
    ushort _expectedSerialResponses = 0;
    #endif

    static void SerialTask(void* args)
    {
        SerialInterface* self = static_cast<SerialInterface*>(args);

        while (Serial.available())
        {
            char c = Serial.read();
            Serial.write(c);
            self->_serialBuffer.push_back(c);
        }
    }

    static void ProcessTask(void* args)
    {
        SerialInterface* self = static_cast<SerialInterface*>(args);

        #if defined(ESP32)
        SerialTask(args);
        #endif

        #ifdef DEBUG
        auto gpsSerial = *self->_gps->DebugSerial;
        auto gsmSerial = *self->_gsm->DebugSerial;
        #endif

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
                Serial.println(F("pong"));
            }
            else if (str.startsWith("reset"))
            {
                ESP.restart();
            }
            #ifdef DEBUG
            else if (str.startsWith("gps toggle relay"))
            {
                Serial.print(F("Toggled GPS serial relay to: "));
                Serial.println(!self->_gps->DebugRelaySerial);
                self->_gps->DebugRelaySerial = !self->_gps->DebugRelaySerial;
            }
            else if (str.startsWith("gps location"))
            {
                Serial.print(F("\nLat: "));
                Serial.println(self->_gps->TinyGps.location.lat(), 6);
                Serial.print(F("Lng: "));
                Serial.println(self->_gps->TinyGps.location.lng(), 6);
            }
            else if (str.startsWith("gsm connect"))
            {
                self->_gsm->Connect();
            }
            #ifdef NET_MQTT
            else if (str.startsWith("mqtt connect"))
            {
                self->_mqtt->Connect();
            }
            else if (str.startsWith("mqtt publish"))
            {
                self->_mqtt->Send(MQTT_TOPIC, str.c_str());
            }
            #endif
            #ifdef NET_HTTP
            else if (str.startsWith("http test"))
            {
                HTTP::SRequest request =
                {
                    .method = HTTP_METHOD,
                    .path = HTTP_PATH,
                    .query = { { "millis", String(millis()).c_str() } }
                };
                self->_http->ProcessRequest(request);
                Serial.print(F("Request response code: "));
                Serial.println(request.responseCode);
                if (!request.responseBody.isEmpty())
                {
                    Serial.println(F("Response body:"));
                    Serial.println(request.responseBody);
                }
            }
            #endif
            #endif
            else
            {
                #ifdef DEBUG
                //Otherwise relay the input to the other serial busses.
                self->_expectedSerialResponses++;

                // if (gpsSerial != nullptr)
                //     gpsSerial->print(str);

                if (self->_gsm != nullptr)
                    gsmSerial->print(str);
                #endif
            }
        }

        #ifdef DEBUG
        //Only read from the other serial busses if we have directly written anything to them. Otherwise leave them to be read by their owning classes.
        if (self->_expectedSerialResponses == 0)
            return;

        if (gsmSerial != nullptr)
        {
            if (gpsSerial->available())
                self->_expectedSerialResponses--;

            while (gsmSerial->available())
            {
                char c = gsmSerial->read();
                Serial.print(F(c));
            }
        }
        #endif
    }

public:
    SerialInterface(GPS* gps, GSM* gsm, MQTT* mqtt, HTTP* http)
    : _gps(gps), _gsm(gsm), _mqtt(mqtt), _http(http)
    {}

    void Begin()
    {
        #if defined(ESP32)
        if (_processTaskId != 0)
            return;
        _processTaskId = Scheduler::Add(100, ProcessTask, this);
        #elif defined(ESP8266)
        if (_serialTaskId != 0)
            return;
        _serialTaskId = Scheduler::Add(100, SerialTask, this);
        #endif
    }

    void End()
    {
        #if defined(ESP32)
        if (_processTaskId == 0)
            return;
        Scheduler::Remove(_processTaskId);
        #elif defined(ESP8266)
        if (_serialTaskId == 0)
            return;
        Scheduler::Remove(_serialTaskId);
        #endif
    }

    #if defined(ESP8266)
    void Loop()
    {
        //This makes the code easier to read than old solutions however it does have an added response time to any serial inputs.
        ProcessTask(this);
    }
    #endif
};
