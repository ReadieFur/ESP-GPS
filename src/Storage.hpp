#pragma once

#define nameof(n) #n

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "Board.h"
#include "Config.h"
#include <mutex>

class Storage
{
private:
    static const char* _filename;

    static bool _initialized;
    static std::mutex _mutex;

public:
    static JsonDocument Cache;

    static bool Init()
    {
        _mutex.lock();

        //Set all default values to fall back on incase storage initialization fails.
        Cache[nameof(MODEM_APN)] = MODEM_APN;
        Cache[nameof(MODEM_PIN)] = MODEM_PIN;
        Cache[nameof(MODEM_USERNAME)] = MODEM_USERNAME;
        Cache[nameof(MODEM_PASSWORD)] = MODEM_PASSWORD;
        Cache[nameof(MQTT_BROKER)] = MQTT_BROKER;
        Cache[nameof(MQTT_PORT)] = MQTT_PORT;
        Cache[nameof(MQTT_CLIENT_ID)] = MQTT_CLIENT_ID;
        Cache[nameof(MQTT_USERNAME)] = MQTT_USERNAME;
        Cache[nameof(MQTT_PASSWORD)] = MQTT_PASSWORD;
        Cache[nameof(BATTERY_CRIT_SLEEP)] = BATTERY_CRIT_SLEEP;
        Cache[nameof(BATTERY_LOW_INTERVAL)] = BATTERY_LOW_INTERVAL;
        Cache[nameof(BATTERY_OK_INTERVAL)] = BATTERY_OK_INTERVAL;
        Cache[nameof(BATTERY_CHRG_INTERVAL)] = BATTERY_CHRG_INTERVAL;
        Cache[nameof(AP_SSID)] = AP_SSID;

        if (!SPIFFS.begin(true))
            SerialMon.println("Failed to initialize SPIFFS.");

        if (SPIFFS.exists(_filename))
        {
            fs::File file = SPIFFS.open(_filename, FILE_READ, true);
            if (!file)
            {
                file.close();
                _mutex.unlock();
                SerialMon.println("Failed to initialize config file.");
                return false;
            }

            DeserializationError err = deserializeJson(Cache, file);
            file.close();
            if (err != DeserializationError::Ok)
            {
                _mutex.unlock();
                SerialMon.printf("Failed to load config file: %i", err);
                return false;
            }
        }
        else
        {
            fs::File file = SPIFFS.open(_filename, FILE_WRITE);
            if (!file)
            {
                Cache.clear();
                _mutex.unlock();
                SerialMon.println("Failed to initialize config file.");
                return false;
            }

            serializeJson(Cache, file);
            file.close();
        }

        _initialized = true;
        _mutex.unlock();
        SerialMon.println("Successfully loaded configuration file.");
        return true;
    }

    static bool Save()
    {
        _mutex.lock();

        if (!_initialized)
        {
            SerialMon.println("Storage not initialized.");
            return false;
        }

        fs::File file = SPIFFS.open(_filename, FILE_WRITE);
        if (!file)
        {
            SerialMon.println("Failed to open file for writing.");
            return false;
        }

        serializeJson(Cache, file);
        file.close();
        
        _mutex.unlock();
        return true;
    }

    static void ResetAllConfig()
    {
        _mutex.lock();

        if (!_initialized)
            SerialMon.println("Storage not initialized.");
        else if (!SPIFFS.remove(_filename))
            SerialMon.println("Failed to delete config file.");
        
        _mutex.unlock();

        SerialMon.println("Reset config, restarting...");

        ESP.restart();
    }

    // template<typename T>
    // T Get(const char* key)
    // {
    //     // auto value = Cache[key];
    //     // return !value.is<T>() ? value.as<T>() : default(T);
    //     return Cache[key].as<T>();
    // }
};

const char* Storage::_filename = "/config.json";

bool Storage::_initialized = false;
JsonDocument Storage::Cache;
std::mutex Storage::_mutex;

#define GetConfig(T, key) Storage::Cache[#key].as<T>()
#define SetConfig(key, value) Storage::Cache[#key] = value;
