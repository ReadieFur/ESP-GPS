#pragma once

//https://registry.platformio.org/libraries/ayushsharma82/ElegantOTA/examples/Demo/Demo.ino

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "Config.h"

class OTA
{
private:
    static WebServer server;
    static ulong otaProgressMillis;

    static void OnOTAStart()
    {
        SerialMon.println("OTA update started!");
    }

    static void OnOTAProgress(size_t current, size_t final)
    {
        ulong now = millis();
        if (now - otaProgressMillis > 1000)
        {
            SerialMon.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
            otaProgressMillis = now;
        }
    }

    static void OnOTAEnd(bool success)
    {
        if (success)
            SerialMon.println("OTA update finished successfully!");
        else
            SerialMon.println("There was an error during OTA update!");
    }

public:
    static void Init()
    {
        SerialMon.begin(115200);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, emptyString, 1, 1);

        ElegantOTA.begin(&server);
        ElegantOTA.onStart(OnOTAStart);
        ElegantOTA.onProgress(OnOTAProgress);
        ElegantOTA.onEnd(OnOTAEnd);

        server.begin();
    }

    static void Loop()
    {
        delay(2000); //Allow time for a client to connect.
        server.handleClient();
        while (WiFi.softAPgetStationNum() > 0)
            ElegantOTA.loop();
    }
};

WebServer OTA::server = WebServer(80);
ulong OTA::otaProgressMillis = 0;
