#pragma once

#include <SoftwareSerial.h>
#ifdef DEBUG
#include <StreamDebugger.h>
#endif
#include "Scheduler.hpp"
#include <TinyGSM.h>
#include <TinyGsmClient.h>
#include <map>
#include <set>

class GSM
{
private:
    const char *_apn, *_pin, *_username, *_password;
    #ifdef DEBUG
    SoftwareSerial* _rawModemSerial;
    StreamDebugger* _modemSerial;
    #else
    SoftwareSerial* _modemSerial;
    #endif
    #if defined(ESP32)
    ulong _validateConnectionTaskId;
    #elif defined(ESP8266)
    ulong _connectionCheckInterval;
    ulong _lastValidateConnectionTime = 0;
    #endif
    std::map<TinyGsmClient*, ushort> _clients;

    ushort ValidateConnection(uint32_t waitForNetworkTimeout = 5000U)
    {
        if (!Modem->isNetworkConnected())
        {
            if (!Modem->restart(_pin))
            {
                Serial.println("Failed to restart modem.");
                return 1;
            }
            
            if (!Modem->waitForNetwork(waitForNetworkTimeout))
            {
                Serial.println("Modem failed to connect to network after timeout.");
                return 2;
            }
        }

        if (Modem->isGprsConnected())
        {
            if (!Modem->gprsConnect(_apn, _username, _password))
            {
                Serial.println("Modem failed to connect to GPRS network.");
                return 3;
            }
        }

        #ifdef DEBUG
        Serial.println("GSM alive.");
        #endif

        return 0;
    }

public:
    TinyGsm* Modem;

    GSM(uint8_t rxPin, uint8_t txPin, const char* apn, const char* pin = "", const char* username = "", const char* password = "", ulong connectionCheckInterval = 10000U)
    : _apn(apn), _pin(strlen(pin) ? pin : nullptr), _username(username), _password(password)
    {
        #ifdef DEBUG
        _rawModemSerial = new SoftwareSerial(rxPin, txPin);
        _rawModemSerial->begin(9600);
        _modemSerial = new StreamDebugger(*_rawModemSerial, Serial);
        #else
        _modemSerial = new SoftwareSerial(rxPin, txPin);
        _modemSerial->begin(9600);
        #endif

        Modem = new TinyGsm(*_modemSerial);

        //Only fail if the initial setup of the modem fails, network can timeout and be retried later.
        if (ValidateConnection(1000U) == 1)
        {
            Serial.println("Failed to initialize modem.");
            abort();
        }

        //I cannot seem to call any TinyGsm methods from within my ESP8266 ticker scheduler, so for this board I will call the ValidateConnection method externally from the main loop.
        #if defined(ESP32)
        _validateConnectionTaskId = Scheduler::Add(connectionCheckInterval, [](void* args){ static_cast<GSM*>(args)->ValidateConnection(); }, this);
        #elif defined(ESP8266)
        _connectionCheckInterval = connectionCheckInterval;
        #endif
    }

    ~GSM()
    {
        #if defined(ESP32)
        Scheduler::Remove(_validateConnectionTaskId);
        #endif

        //TODO: Clean up all GSM connections here (preferably by API calls, otherwise via manual AT commands).
        for (auto &&client : _clients)
            DestroyClient(client.first);
        Modem->gprsDisconnect();
        delete Modem;

        #ifdef DEBUG
        _rawModemSerial->end();        
        delete _rawModemSerial;
        #else
        _modemSerial->end();
        #endif
        delete _modemSerial;
    }

    TinyGsmClient* CreateClient()
    {
        if (_clients.size() >= TINY_GSM_MUX_COUNT)
        {
            Serial.println("Max number of GSM clients already instantiated.");
            return nullptr;
        }

        //Find the first free ID to use for the mux value.
        ushort firstFreeId = 0;
        std::set<ushort> usedIds;
        for (auto &&client : _clients)
            usedIds.insert(client.second);
        while (usedIds.find(firstFreeId) != usedIds.end())
            firstFreeId++;

        //firstFreeId shouldn't exceed TINY_GSM_MUX_COUNT here.

        TinyGsmClient* client = new TinyGsmClient(*Modem, firstFreeId);
        if (client == nullptr)
        {
            Serial.println("Failed to create GSM client object.");
            return nullptr;
        }

        _clients.insert({client, firstFreeId});
        return client;
    }

    void DestroyClient(TinyGsmClient* client)
    {
        if (_clients.find(client) != _clients.end())
            _clients.erase(client);
        delete client;
    }

    //Only required on ESP8266.
    void Loop()
    {
        #if defined(ESP8266)
        if (millis() - _lastValidateConnectionTime > _connectionCheckInterval)
        {
            //This is blocking.
            ValidateConnection();
            _lastValidateConnectionTime = millis();
        }
        #endif
    }
};
