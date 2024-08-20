#pragma once

#include "Config.h"
#include "Helpers.h"
#include <esp32-hal-gpio.h>
#include <SoftwareSerial.h>
#include <TinyGsmClient.h>
#ifdef _DEBUG
  #include <StreamDebugger.h>
#endif

class Modem
{
private:
    #ifdef _DEBUG
    SoftwareSerial* _modemSerial;
    StreamDebugger* _modemDebugger;
    #endif

public:
    TinyGsm* _tinyGsm;

    Modem(uint8_t rxPin, uint8_t txPin, uint8_t rstPin, uint8_t dtrPin = NULL, uint8_t ringPin = NULL, const char* simPin = NULL)
    {
        pinMode(rstPin, OUTPUT);
        digitalWrite(rstPin, HIGH);
        if (dtrPin != NULL)
        {
            pinMode(dtrPin, OUTPUT);
            digitalWrite(dtrPin, LOW);
        }
        if (ringPin != NULL)
        {
            pinMode(ringPin, INPUT);
        }

        _modemSerial = new SoftwareSerial();
        _modemSerial->begin(115200, SWSERIAL_8N1, rxPin, txPin);

        #ifdef _DEBUG
        _modemDebugger = new StreamDebugger(*_modemSerial, StdSerial);
        _tinyGsm = new TinyGsm(*_modemDebugger);
        #else
        _tinyGsm = new TinyGsm(*_modemSerial);
        #endif

        _tinyGsm->restart();

        if (strlen(simPin) && _tinyGsm->getSimStatus() != 3)
            _tinyGsm->simUnlock(simPin);

        //Configure SMS storage.
        _tinyGsm->sendAT(GF("+CPMS=\"SM\",\"SM\",\"SM\""));
        _tinyGsm->waitResponse();
        //Set SMS text mode.
        _tinyGsm->sendAT(GF("+CMGF=1"));
        _tinyGsm->waitResponse();
        //Enable SMS notifications.
        _tinyGsm->sendAT(GF("+CNMI=2,1,0,0,0"));
        _tinyGsm->waitResponse();
    }

    ~Modem()
    {
        delete _tinyGsm;
        #ifdef _DEBUG
        delete _modemDebugger;
        #endif
        delete _modemSerial;
    }
};
