#pragma once

#include <SoftwareSerial.h>
#ifdef DEBUG
#include <StreamDebugger.h>
#endif
#include <TinyGSM.h>

class GSM
{
private:
    #ifdef DEBUG
    SoftwareSerial* _rawModemSerial;
    StreamDebugger* _modemSerial;
    #else
    SoftwareSerial* _modemSerial;
    #endif

public:
    TinyGsm* Modem;

    GSM(uint8_t rxPin, uint8_t txPin, const char* pin = "")
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

        if (strlen(pin) && Modem->getSimStatus() != 3 && !Modem->simUnlock(pin))
        {
            Serial.println("Failed to unlock SIM.");
            abort();
        }
    }

    ~GSM()
    {
        //TODO: Clean up all GSM connections here (preferably by API calls, otherwise via manual AT commands).
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
};
