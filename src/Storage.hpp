#pragma once

#define nameof(n) #n
#define Storage_SetDefault(kv) Cache[#kv] = kv

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "Board.h"
#include "Config.h"
#include <mutex>
#include <list>
#include <algorithm>

class Storage
{
private:
    static const char* _filename;
    static std::list<const char*> _sensitiveEntries;

    static bool _initialized;
    static std::mutex _mutex;

public:

    static JsonDocument Cache;

    static bool Init()
    {
        _mutex.lock();

        //Set all default values to fall back on incase storage initialization fails.
        Storage_SetDefault(MODEM_APN);
        Storage_SetDefault(MODEM_PIN);
        Storage_SetDefault(MODEM_USERNAME);
        Storage_SetDefault(MODEM_PASSWORD);
        Storage_SetDefault(MQTT_BROKER);
        Storage_SetDefault(MQTT_PORT);
        Storage_SetDefault(MQTT_CLIENT_ID);
        Storage_SetDefault(MQTT_USERNAME);
        Storage_SetDefault(MQTT_PASSWORD);
        Storage_SetDefault(BATTERY_CRIT_SLEEP);
        Storage_SetDefault(BATTERY_LOW_INTERVAL);
        Storage_SetDefault(BATTERY_OK_INTERVAL);
        Storage_SetDefault(BATTERY_CHRG_INTERVAL);
        Storage_SetDefault(MOTION_SENSITIVITY);
        Storage_SetDefault(MOTION_DURATION);
        Storage_SetDefault(AP_SSID);

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

    static bool IsSensitive(const char* key)
    {
        return std::find(_sensitiveEntries.begin(), _sensitiveEntries.end(), key) != _sensitiveEntries.end();
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
std::list<const char*> Storage::_sensitiveEntries =
{
    nameof(MQTT_PASSWORD),
    nameof(MODEM_PASSWORD)
};

bool Storage::_initialized = false;
JsonDocument Storage::Cache;
std::mutex Storage::_mutex;

#define GetConfig(T, key) Storage::Cache[#key].as<T>()
#define SetConfig(key, value) Storage::Cache[#key] = value;
