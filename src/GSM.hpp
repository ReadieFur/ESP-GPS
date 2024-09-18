#pragma once

#include "Board.h"
#include "Config.h"
#include <Client.h>

#define TINY_GSM_DEBUG SerialMon
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
    static StreamDebugger debugger;
    #endif

public:
    static TinyGsm modem;
    static TinyGsmClient client;

    static void Init()
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
        delay(100);
        digitalWrite(MODEM_PWRKEY, HIGH);
        delay(1000);
        digitalWrite(MODEM_PWRKEY, LOW);

        //Set ring pin input.
        pinMode(MODEM_RING, INPUT_PULLUP);

        SerialMon.println("Start modem...");
        delay(3000);
        // Restart takes quite some time
        // To skip it, call init() instead of restart()
        DBG("Initializing modem...");
        if (!modem.init())
        {
            DBG("Failed to restart modem, delaying 10s and retrying");
            return;
        }

        String name = modem.getModemName();
        DBG("Modem Name:", name);

        String modemInfo = modem.getModemInfo();
        DBG("Modem Info:", modemInfo);

        #if TINY_GSM_USE_GPRS
        // Unlock your SIM card with a PIN if needed
        if (MODEM_PIN && modem.getSimStatus() != 3)
            modem.simUnlock(MODEM_PIN);
        #endif

        #if TINY_GSM_USE_WIFI
        // Wifi connection parameters must be set before waiting for the network
        SerialMon.print(F("Setting SSID/password..."));
        if (!modem.networkConnect(MODEM_SSID, MODEM_PASSWORD))
        {
            SerialMon.println(" fail");
            delay(10000);
            return;
        }
        SerialMon.println(" success");
        #endif

        #if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
        // The XBee must run the gprsConnect function BEFORE waiting for network!
        modem.gprsConnect(apn, gprsUser, gprsPass);
        #endif

        SerialMon.print("Waiting for network...");
        if (!modem.waitForNetwork())
        {
            SerialMon.println(" fail");
            delay(10000);
            return;
        }
        SerialMon.println(" success");

        if (modem.isNetworkConnected())
            SerialMon.println("Network connected");

        #if TINY_GSM_USE_GPRS
        // GPRS connection parameters are usually set after network registration
        SerialMon.print(F("Connecting to "));
        SerialMon.print(MODEM_APN);
        if (!modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
        {
            SerialMon.println(" fail");
            delay(10000);
            return;
        }
        SerialMon.println(" success");

        if (modem.isGprsConnected())
            SerialMon.println("GPRS connected");
        #endif
    }

    static void Loop()
    {
        // Make sure we're still registered on the network
        if (!modem.isNetworkConnected())
        {
            SerialMon.println("Network disconnected");
            if (!modem.waitForNetwork(180000L, true))
            {
                SerialMon.println(" fail");
                delay(10000);
                return;
            }
            if (modem.isNetworkConnected())
            {
                SerialMon.println("Network re-connected");
            }

            #if TINY_GSM_USE_GPRS
            // and make sure GPRS/EPS is still connected
            if (!modem.isGprsConnected())
            {
                SerialMon.println("GPRS disconnected!");
                SerialMon.print("Connecting to ");
                SerialMon.print(MODEM_APN);
                if (!modem.gprsConnect(MODEM_APN, MODEM_USERNAME, MODEM_PASSWORD))
                {
                    SerialMon.println(" fail");
                    delay(10000);
                    return;
                }
                if (modem.isGprsConnected())
                    SerialMon.println("GPRS reconnected");
            }
            #endif
        }
    }
};

#ifdef DUMP_AT_COMMANDS
StreamDebugger GSM::debugger(SerialAT, SerialMon);
TinyGsm GSM::modem(GSM::debugger);
#else
TinyGsm GSM::modem(SerialAT);
#endif
TinyGsmClient GSM::client(GSM::modem);
