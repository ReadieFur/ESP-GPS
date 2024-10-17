#pragma once

#include "Board.h"
#include "Config.h"
#include <Client.h>

// #ifdef DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG SerialMon
// #endif
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#include <TinyGsmClient.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif

class GSM
{
private:
    #ifdef DUMP_AT_COMMANDS
    static StreamDebugger _debugger;
    #endif

public:
    static TinyGsm Modem;
    static TinyGsmClient Client;

    static bool Init()
    {
        SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

        //Power on modem.
        pinMode(MODEM_POWERON, OUTPUT);
        digitalWrite(MODEM_POWERON, HIGH);

        //Reset modem.
        pinMode(MODEM_RESET, OUTPUT);
        digitalWrite(MODEM_RESET, LOW);
        pinMode(MODEM_PWRKEY, OUTPUT);
        digitalWrite(MODEM_PWRKEY, LOW);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(MODEM_PWRKEY, HIGH);
        vTaskDelay(pdMS_TO_TICKS(1000));
        digitalWrite(MODEM_PWRKEY, LOW);

        //Set ring pin input.
        pinMode(MODEM_RING, INPUT_PULLUP);

        SerialMon.println("Start modem...");
        //Wait for modem to be ready.
        vTaskDelay(pdMS_TO_TICKS(3000));
        //Restart takes quite some time, to skip it, call init() instead of restart().
        DBG("Initializing modem...");
        if (!Modem.init())
        {
            DBG("Failed to restart modem.");
            return false;
        }

        String name = Modem.getModemName();
        DBG("Modem Name:", name);

        String modemInfo = Modem.getModemInfo();
        DBG("Modem Info:", modemInfo);

        #if TINY_GSM_USE_GPRS
        //Unlock your SIM card with a PIN if needed.
        if (MODEM_PIN && Modem.getSimStatus() != 3)
            Modem.simUnlock(MODEM_PIN);
        #endif

        SerialMon.print("Waiting for network...");
        if (!Modem.waitForNetwork())
        {
            SerialMon.println(" fail");
            return true;
        }
        SerialMon.println(" success");

        if (Modem.isNetworkConnected())
            SerialMon.println("Network connected");

        #if TINY_GSM_USE_GPRS
        //GPRS connection parameters are usually set after network registration.
        SerialMon.print(F("Connecting to "));
        SerialMon.print(MODEM_APN);
        if (!Modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
        {
            SerialMon.println(" fail");
            return true;
        }
        SerialMon.println(" success");

        if (Modem.isGprsConnected())
            SerialMon.println("GPRS connected");
        #endif

        return true;
    }

    static bool Loop()
    {
        Modem.sleepEnable(false);

        //Make sure we're still registered on the network.
        if (Modem.isNetworkConnected())
            return true;
        
        SerialMon.print("Network disconnected, reconnecting...");
        if (!Modem.waitForNetwork(60000L, true))
        {
            SerialMon.println(" fail");
            return false;
        }
        if (Modem.isNetworkConnected())
        {
            SerialMon.println(" success");
        }

        //And make sure GPRS/EPS is still connected.
        if (Modem.isGprsConnected())
            return true;
        
        SerialMon.println("GPRS disconnected!");
        SerialMon.print("Connecting to ");
        SerialMon.print(MODEM_APN);
        if (!Modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
        {
            SerialMon.println(" fail");
            return false;
        }
        if (Modem.isGprsConnected())
            SerialMon.println("GPRS reconnected");

        return true;
    }
};

#ifdef DUMP_AT_COMMANDS
StreamDebugger GSM::_debugger(SerialAT, SerialMon);
TinyGsm GSM::Modem(GSM::_debugger);
#else
TinyGsm GSM::modem(SerialAT);
#endif
TinyGsmClient GSM::Client(GSM::Modem);
