#pragma once

#ifdef DEBUG
#define GSM_RELAY_SERIAL
#endif

#include <SoftwareSerial.h>
#include "Scheduler.hpp"
#include <TinyGSM.h>
#include <TinyGsmClient.h>
#include <map>
#include <set>
#ifdef DEBUG
#include <StreamDebugger.h>
#endif

class GSM
{
private:
    const char *_apn, *_pin, *_username, *_password;
    #ifdef GSM_RELAY_SERIAL
    SoftwareSerial* _rawModemSerial;
    StreamDebugger* _modemSerial;
    #else
    SoftwareSerial* _modemSerial;
    #endif
    #if defined(ESP32)
    ulong _validateConnectionTaskId;
    #endif
    std::map<TinyGsmClient*, ushort> _clients;

public:
    TinyGsm* Modem;
    #ifdef DEBUG
    #ifdef GSM_RELAY_SERIAL
    SoftwareSerial** DebugSerial = &_rawModemSerial;
    #else
    SoftwareSerial** DebugSerial = &_modemSerial;
    #endif
    #endif

    GSM(uint8_t rxPin, uint8_t txPin, const char* apn, const char* pin = "", const char* username = "", const char* password = "")
    : _apn(apn), _pin(pin), _username(username), _password(password)
    {
        #ifdef GSM_RELAY_SERIAL
        _rawModemSerial = new SoftwareSerial(rxPin, txPin);
        _rawModemSerial->begin(9600);
        _modemSerial = new StreamDebugger(*_rawModemSerial, Serial);
        #else
        _modemSerial = new SoftwareSerial(rxPin, txPin);
        _modemSerial->begin(9600);
        #endif

        Modem = new TinyGsm(*_modemSerial);
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

        #ifdef GSM_RELAY_SERIAL
        _rawModemSerial->end();        
        delete _rawModemSerial;
        #else
        _modemSerial->end();
        #endif
        delete _modemSerial;
    }

    #if defined(ESP32)
    /// @brief Enable task based automatic connection checking.
    /// @param connectionCheckInterval The time between checks of the connection. Set to 0 to disable automatic checks.
    /// @note Only available on ESP32.
    void ConfigureAutomaticConnectionCheck(ulong connectionCheckInterval)
    {
        if (connectionCheckInterval == 0)
        {
            Scheduler::Remove(_validateConnectionTaskId);
            _validateConnectionTaskId = 0;
            return;
        }
        
        if (_validateConnectionTaskId != 0)
            Scheduler::Remove(_validateConnectionTaskId);

        //I cannot seem to call any TinyGsm methods from within my ESP8266 ticker scheduler, so for this board I will call the ValidateConnection method externally from the main loop.
        _validateConnectionTaskId = Scheduler::Add(connectionCheckInterval, [](void* args){ static_cast<GSM*>(args)->Connect(); }, this);
    }
    #endif

    /// @brief Connect to GPRS if not already connected.
    /// @param timeout Timeout for checking the network status.
    /// @return 0 on success, 1 for GSM/GPRS/LTE error, 2 for GPRS/EPS error.
    ushort Connect(uint32_t timeout = 5000U)
    {
        if (!Modem->isNetworkConnected())
        {
            if (!Modem->restart(_pin))
            {
                Serial.println("Failed to restart modem.");
                return 1;
            }
            
            if (!Modem->waitForNetwork(timeout))
            {
                Serial.println("Modem failed to connect to network after timeout.");
                return 1;
            }
        }

        if (Modem->isGprsConnected())
        {
            // Modem->gprsDisconnect();

            if (!Modem->gprsConnect(_apn, _username, _password))
            {
                Serial.println("Modem failed to connect to GPRS network.");
                return 2;
            }
        }

        #ifdef DEBUG
        Serial.println("GSM alive.");
        #endif

        return 0;
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
};
