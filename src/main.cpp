#include <Arduino.h>
#include "Board.h"
#include "Config.h"
#include <TinyGPS++.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <vector>
#include <algorithm>
#include <numeric>

#define DUMP_AT_COMMANDS
#define DUMP_GPS_COMMANDS
#if defined(DUMP_AT_COMMANDS) || defined(DUMP_GPS_COMMANDS)
#include <StreamDebugger.h>
#endif

TinyGPSPlus gps;

#define TINY_GSM_DEBUG SerialMon
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#ifdef DUMP_AT_COMMANDS
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient  mqtt(client);
uint32_t lastReconnectAttempt = 0;

uint32_t voltage_interval = 0;

void light_sleep(uint32_t ms)
{
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void deep_sleep(uint32_t ms)
{
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_deep_sleep_start();
}

uint32_t getBatteryVoltage()
{
    // Calculate the average power data
    std::vector<uint32_t> data;
    for (int i = 0; i < 30; ++i)
    {
        uint32_t val = analogReadMilliVolts(BOARD_BAT_ADC);
        // Serial.printf("analogReadMilliVolts : %u mv \n", val * 2);
        data.push_back(val);
        delay(30);
    }
    std::sort(data.begin(), data.end());
    data.erase(data.begin());
    data.pop_back();
    int sum = std::accumulate(data.begin(), data.end(), 0);
    double average = static_cast<double>(sum) / data.size();
    return  average * 2;
}

void displayInfo()
{
    SerialMon.print("Location: ");
    if (gps.location.isValid())
    {
        SerialMon.print(gps.location.lat(), 6);
        SerialMon.print(",");
        SerialMon.print(gps.location.lng(), 6);
    }
    else
    {
        SerialMon.print("INVALID");
    }

    SerialMon.print("  Date/Time: ");
    if (gps.date.isValid())
    {
        SerialMon.print(gps.date.month());
        SerialMon.print("/");
        SerialMon.print(gps.date.day());
        SerialMon.print("/");
        SerialMon.print(gps.date.year());
    }
    else
    {
        SerialMon.print("INVALID");
    }

    SerialMon.print(" ");
    if (gps.time.isValid())
    {
        if (gps.time.hour() < 10) SerialMon.print("0");
        SerialMon.print(gps.time.hour());
        SerialMon.print(":");
        if (gps.time.minute() < 10) SerialMon.print("0");
        SerialMon.print(gps.time.minute());
        SerialMon.print(":");
        if (gps.time.second() < 10) SerialMon.print("0");
        SerialMon.print(gps.time.second());
        SerialMon.print(".");
        if (gps.time.centisecond() < 10) SerialMon.print("0");
        SerialMon.print(gps.time.centisecond());
    }
    else
    {
        SerialMon.print("INVALID");
    }

    SerialMon.println();
}

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();
}

boolean mqttConnect()
{
    SerialMon.print("Connecting to ");
    SerialMon.print(MQTT_BROKER);

    // Connect to MQTT Broker
    boolean status = mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);

    if (status == false)
    {
        SerialMon.println(" fail");
        return false;
    }
    SerialMon.println(" success");

    mqtt.publish(MQTT_TOPIC, "GsmClientTest started");
    mqtt.subscribe(MQTT_TOPIC);

    return mqtt.connected();
}

void setup()
{
    SerialMon.begin(115200);
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    uint32_t battery_voltage_mv = getBatteryVoltage();
    // If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
    if (battery_voltage_mv < BATTERY_LOW_LEVEL)
    {
        Serial.printf("Battery voltage is too low ,%u mv, entering sleep mode\n", battery_voltage_mv);
        deep_sleep(BATTERY_LOW_SLEEP_TIME);
    }
    Serial.printf("Battery voltage is %u mv\n", battery_voltage_mv);
    
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

    // MQTT Broker setup
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
}

void loop()
{
    #pragma Region battery
    if (millis() > voltage_interval)
    {
        // Check the battery voltage every 30 seconds
        voltage_interval = millis() + 30000;

        uint32_t battery_voltage_mv = getBatteryVoltage();

        // If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.

        if (battery_voltage_mv < BATTERY_LOW_LEVEL) {

            Serial.printf("Battery voltage is too low ,%u mv, entering sleep mode\n", battery_voltage_mv);

            // Turn off the modem
            modem.poweroff();

            // Sleep esp32
            deep_sleep(BATTERY_LOW_SLEEP_TIME); //60 minute

        } else if (battery_voltage_mv < BATTERY_WARN_LEVEL) {

            Serial.println("Battery voltage reaches the warning voltage");

        }

    }
    #pragma endregion

    #pragma region SerialMon
    // while (SerialMon.available())
    // {
    //     int c = SerialMon.read();
    //     SerialMon.write(c); //Relay the character back to the terminal (required so you can see what you are typing).
    //     SerialAT.write(c);
    // }
    // while (SerialAT.available())
    //     SerialMon.write(SerialAT.read());
    #pragma endregion

    #pragma region GPS
    while (SerialGPS.available())
    {
        int c = SerialGPS.read();
        #ifdef DUMP_GPS_COMMANDS
        SerialMon.write(c);
        #endif
        if (gps.encode(c))
            displayInfo();
    }
    #pragma endregion

    #pragma region GSM
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

    if (!mqtt.connected())
    {
        SerialMon.println("=== MQTT NOT CONNECTED ===");
        //Reconnect every 10 seconds.
        uint32_t t = millis();
        if (t - lastReconnectAttempt > 10000L)
        {
            lastReconnectAttempt = t;
            if (mqttConnect())
                lastReconnectAttempt = 0;
        }
        delay(100);
        return;
    }

    mqtt.loop();
    #pragma endregion

    delay(1);
}
