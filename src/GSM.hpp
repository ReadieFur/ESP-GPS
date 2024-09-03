#pragma once

#include <SoftwareSerial.h>
#ifdef DEBUG
#include <StreamDebugger.h>
#endif

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
    GSM(uint8_t rxPin, uint8_t txPin)
    {
        #ifdef DEBUG
        _rawModemSerial = new SoftwareSerial(rxPin, txPin);
        _rawModemSerial->begin(9600);
        _modemSerial = new StreamDebugger(*_rawModemSerial, Serial);
        #else
        _modemSerial = new SoftwareSerial(rxPin, txPin);
        _modemSerial->begin(9600);
        #endif
    }

    ~GSM()
    {
        #ifdef DEBUG
        _rawModemSerial->end();        
        delete _rawModemSerial;
        #else
        _modemSerial->end();
        #endif
        delete _modemSerial;
    }
};
