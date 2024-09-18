#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(ESP8266)
#include "TimedLoop.hpp"
#endif
#include "Config.h"
#include "Boards.h"
#include "CustomBoard.h"
#include "Scheduler.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include "SerialInterface.hpp"
#include <map>
#include <WString.h>

GPS* gps;
GSM* gsm;
SerialInterface* serialInterface;

#ifdef NET_MQTT
#include "MQTT.hpp"
TinyGsmClient* mqttClient;
MQTT* mqtt;
#if defined(ESP8266)
TimedLoop mqttLoop(100);
#endif
#endif

#ifdef NET_HTTP
#include "HTTP.hpp"
TinyGsmClient* httpClient;
HTTP* http;
#endif

#if defined(ESP32)
ulong statusUpdateCallbackId = 0;
#elif defined(ESP8266)
TimedLoop statusUpdateCallbackLoop(SEND_INTERVAL);
TimedLoop connectionCheckLoop(1000);
#endif

void StatusUpdateCallback(void*)
{
    #ifdef DEBUG
    Serial.println(F("Preparing to send data..."));
    #endif

    std::map<String, String> data;

    #ifdef SEND_GPS_LOCATION
    if (gps->TinyGps.location.isValid())
    {
        data.insert({"loc_lat", String(gps->TinyGps.location.lat())});
        data.insert({"loc_lng", String(gps->TinyGps.location.lng())});
        data.insert({"loc_quality", String(gps->TinyGps.location.FixQuality())});
        data.insert({"loc_age", String(gps->TinyGps.location.age())});
    }
    #endif

    #ifdef SEND_GPS_DATE
    if (gps->TinyGps.date.isValid())
    {
        data.insert({"date", String(gps->TinyGps.date.value())});
        data.insert({"date_age", String(gps->TinyGps.date.value())});
    }
    #endif

    #ifdef SEND_GPS_TIME
    if (gps->TinyGps.time.isValid())
    {
        data.insert({"time", String(gps->TinyGps.time.value())});
        data.insert({"time_age", String(gps->TinyGps.time.age())});
    }
    #endif

    #ifdef SEND_GPS_SPEED
    if (gps->TinyGps.speed.isValid())
    {
        data.insert({"mps", String(gps->TinyGps.speed.mps())});
        data.insert({"mps_age", String(gps->TinyGps.speed.age())});
    }
    #endif

    #ifdef SEND_GPS_COURSE
    if (gps->TinyGps.course.isValid())
    {
        data.insert({"deg", String(gps->TinyGps.course.deg())});
        data.insert({"deg_age", String(gps->TinyGps.course.age())});
    }
    #endif

    #ifdef SEND_GPS_ALTITUDE
    if (gps->TinyGps.altitude.isValid())
    {
        data.insert({"alt", String(gps->TinyGps.altitude.meters())});
        data.insert({"alt_age", String(gps->TinyGps.altitude.age())});
    }
    #endif

    #ifdef SEND_GPS_SATELLITES
    if (gps->TinyGps.satellites.isValid())
    {
        data.insert({"sat", String(gps->TinyGps.satellites.value())});
        data.insert({"sat_age", String(gps->TinyGps.satellites.age())});
    }
    #endif

    #ifdef SEND_GPS_HDOP
    if (gps->TinyGps.hdop.isValid())
    {
        data.insert({"hdop", String(gps->TinyGps.hdop.hdop())});
        data.insert({"hdop_age", String(gps->TinyGps.satellites.age())});
    }
    #endif

    #if defined(SEND_BATTERY_SOC) || defined(SEND_BATTERY_PERCENTAGE) || defined(SEND_BATTERY_VOLTAGE)
    uint8_t batSoc;
    int8_t batPct;
    uint16_t batVlt;
    gsm->Modem->getBattStats(batSoc, batPct, batVlt);
    #ifdef SEND_BATTERY_SOC
    data.insert({"bat_soc", String(batSoc)});
    #endif
    #ifdef SEND_BATTERY_PERCENTAGE
    data.insert({"bat_pct", String(batPct)});
    #endif
    #ifdef SEND_BATTERY_VOLTAGE
    data.insert({"bat_vlt", String(batVlt)});
    #endif
    #endif

    #ifdef SEND_GSM_OPERATOR
    data.insert({"gsm_op", gsm->Modem->getOperator()});
    #endif
    #ifdef SEND_GSM_SIGNAL_STRENGTH
    data.insert({"gsm_rssi", String(gsm->Modem->getSignalQuality())});
    #endif
    #ifdef SEND_GSM_IP
    data.insert({"gsm_ip", gsm->Modem->getLocalIP()});
    #endif

    //Remove any entries where the value is empty, should hopefully be none of them.
    for (auto it = data.begin(); it != data.end(); /*No increment here.*/)
        if (it->second.isEmpty())
            it = data.erase(it);
        else
            ++it;

    //Why would this ever be empty under real world conditions lol?
    if (data.empty())
        return;

    #if defined(DEBUG)
    String logMessage;
    for (auto &&kvp : data)
    {
        logMessage += kvp.first;
        logMessage += '=';
        logMessage += kvp.second;
        logMessage += '\n';
    }
    Serial.println(F("Sending data:"));
    Serial.print(F(logMessage.c_str()));
    #endif

    #ifdef NET_MQTT
    String mqttMessage;
    for (auto &&kvp : data)
    {
        mqttMessage += kvp.first;
        mqttMessage += '=';
        mqttMessage += kvp.second;
        mqttMessage += '\n';
    }
    while (mqttMessage[mqttMessage.length() - 1] == '\n')
        mqttMessage.remove(mqttMessage.length() - 1);
    
    if (!mqtt->Send(MQTT_TOPIC, mqttMessage.c_str()))
    {
        Serial.print(F("Failed to send MQTT message: "));
        Serial.println(mqtt->mqttClient.state());
    }
    #endif

    #ifdef NET_HTTP
    HTTP::SRequest httpRequest =
    {
        .method = HTTP_METHOD,
        .path = HTTP_PATH
    };

    if (HTTP_METHOD == HTTP::EMethod::GET)
    {
        httpRequest.query = data;
    }
    else
    {
        String httpBody;
        for (auto &&kvp : data)
        {
            httpBody += kvp.first;
            httpBody += '=';
            httpBody += kvp.second;
            httpBody += '\n';
        }
        while (httpBody[httpBody.length() - 1] == '\n')
            httpBody.remove(httpBody.length() - 1);

        httpRequest.requestBody = httpBody;
    }

    http->ProcessRequest(httpRequest);
    if (httpRequest.responseCode < 200 || httpRequest.responseCode > 300)
        Serial.println(F("Failed to send HTTP request."));
    #endif
}

void Main()
{
    // //Reset hardware on boot.
    // pinMode(MODEM_POWERON, OUTPUT);
    // digitalWrite(MODEM_POWERON, HIGH);
    // pinMode(MODEM_RESET, OUTPUT);
    // digitalWrite(MODEM_RESET, LOW);
    // pinMode(MODEM_PWRKEY, OUTPUT);
    // digitalWrite(MODEM_PWRKEY, LOW);
    // delay(100);
    // digitalWrite(MODEM_PWRKEY, HIGH);
    // delay(1000);
    // digitalWrite(MODEM_PWRKEY, LOW);

    //This has to be set first before any other objects are initalized as if they write to serial before this the baud is set to something different.
    Serial.begin(9600);

    #ifdef DEBUG
    //Allows time for me to connect to the serial.
    delay(2000);
    #endif

    #ifdef DEBUG
    Serial.println(F("[DEBUG] Main()"));
    //This is in place just for me to make sure the serial hasn't lost connection or the device has stalled.
    Scheduler::Add(5000, [](void*){ Serial.println(F((String("SERIAL_ALIVE_CHECK:") + String((millis() / 1000) - 2)).c_str())); });
    #endif

    gps = new GPS(GPS_TX, GPS_RX);

    gsm = new GSM(MODEM_TX, MODEM_RX, MODEM_APN, MODEM_PIN, MODEM_USERNAME, MODEM_PASSWORD);
    gsm->Connect();
    #if defined(ESP32)
    gsm->ConfigureAutomaticConnectionCheck(100);
    #endif

    #ifdef NET_MQTT
    mqttClient = gsm->CreateClient();
    mqtt = new MQTT(*mqttClient, MQTT_DEVICE_ID, MQTT_BROKER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD);
    mqtt->Subscribe(MQTT_TOPIC);
    #if defined(ESP32)
    mqtt->ConfigureAutomaticConnectionCheck(100);
    mqtt->ConfigureAutomaticCallbackDispatcher(100);
    #elif defined(ESP8266)
    mqttLoop.Callback = [](){ mqtt->Loop(); };
    #endif
    #endif

    #ifdef NET_HTTP
    httpClient = gsm->CreateClient();
    http = new HTTP(*httpClient, HTTP_ADDRESS, HTTP_PORT);
    #endif

    serialInterface = new SerialInterface(
        gps,
        gsm,
        #ifdef NET_MQTT
        mqtt,
        #else
        nullptr,
        #endif
        #ifdef NET_HTTP
        http
        #else
        nullptr
        #endif
    );
    serialInterface->Begin();

    #if defined(ESP32)
    statusUpdateCallbackId = Scheduler::Add(SEND_INTERVAL, StatusUpdateCallback);
    #elif defined(ESP8266)
    // statusUpdateCallbackLoop = TimedLoop<void>(SEND_INTERVAL, [](){ StatusUpdateCallback(nullptr); });
    statusUpdateCallbackLoop.Callback = [](){ StatusUpdateCallback(nullptr); };

    connectionCheckLoop.Callback = []()
    {
        if (gsm->Connect() != 0)
            return;

        #ifdef NET_MQTT
        mqtt->Connect();
        #endif
    };
    #endif
}

#if defined(ESP8266)
void Loop()
{
    serialInterface->Loop();
    connectionCheckLoop.Loop();
    #ifdef NET_MQTT
    mqttLoop.Loop();
    #endif
    statusUpdateCallbackLoop.Loop();
    yield();
}
#endif

#pragma region Entrypoint stuff
#ifdef ARDUINO
void setup()
{
    Main();
}

void loop()
{
    #if defined(ESP32)
    // vPortYield();
    vTaskDelete(NULL);
    #elif defined(ESP8266)
    Loop();
    #endif
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
#pragma endregion
