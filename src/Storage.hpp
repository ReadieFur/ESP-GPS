#pragma once

#define nameof(n) #n
#define _StorageSetDefault(kv) if (Cache[#kv].isNull()) Cache[#kv] = kv

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "Board.h"
#include "Config.h"
#include <mutex>
#include <list>
#include <algorithm>
#include "Helpers.h"
#include <esp_log.h>

namespace ReadieFur::EspGps
{
    class Storage
    {
    private:
        static const char* _filename;
        static std::list<const char*> _sensitiveEntries;

        static bool _initialized;
        static std::mutex _mutex;

        static void SetDefaults()
        {
            //Set all default values to fall back on incase storage initialization fails.
            _StorageSetDefault(MODEM_APN);
            _StorageSetDefault(MODEM_PIN);
            _StorageSetDefault(MODEM_USERNAME);
            _StorageSetDefault(MODEM_PASSWORD);
            _StorageSetDefault(MQTT_BROKER);
            _StorageSetDefault(MQTT_PORT);
            _StorageSetDefault(MQTT_CLIENT_ID);
            _StorageSetDefault(MQTT_USERNAME);
            _StorageSetDefault(MQTT_PASSWORD);
            _StorageSetDefault(MQTT_TOPIC);
            _StorageSetDefault(BATTERY_CRIT_SLEEP);
            _StorageSetDefault(BATTERY_LOW_INTERVAL);
            _StorageSetDefault(BATTERY_OK_INTERVAL);
            _StorageSetDefault(BATTERY_CHRG_INTERVAL);
            _StorageSetDefault(MOTION_SENSITIVITY);
            _StorageSetDefault(MOTION_DURATION);
            _StorageSetDefault(AP_SSID);
        }

    public:
        static JsonDocument Cache;

        static bool Init()
        {
            _mutex.lock();

            if (!SPIFFS.begin(true))
            {
                ESP_LOGE(nameof(Storage), "Failed to initialize SPIFFS.");
                SetDefaults();
                return false;
            }

            if (SPIFFS.exists(_filename))
            {
                fs::File file = SPIFFS.open(_filename, FILE_READ, true);
                if (!file)
                {
                    file.close();
                    _mutex.unlock();
                    ESP_LOGE(nameof(Storage), "Failed to initialize config file.");
                    SetDefaults();
                    return false;
                }

                DeserializationError err = deserializeJson(Cache, file);
                file.close();
                if (err != DeserializationError::Ok)
                {
                    _mutex.unlock();
                    ESP_LOGE(nameof(Storage), "Failed to load config file: %i", err);
                    return false;
                }
                SetDefaults();
            }
            else
            {
                SetDefaults();
                fs::File file = SPIFFS.open(_filename, FILE_WRITE);
                if (!file)
                {
                    Cache.clear();
                    _mutex.unlock();
                    ESP_LOGE(nameof(Storage), "Failed to initialize config file.");
                    return false;
                }

                serializeJson(Cache, file);
                file.close();
            }

            _initialized = true;
            _mutex.unlock();
            ESP_LOGI(nameof(Storage), "Successfully loaded configuration file.");
            return true;
        }

        static bool Save()
        {
            _mutex.lock();

            if (!_initialized)
            {
                ESP_LOGW(nameof(Storage), "Storage not initialized.");
                return false;
            }

            fs::File file = SPIFFS.open(_filename, FILE_WRITE);
            if (!file)
            {
                ESP_LOGE(nameof(Storage), "Failed to open file for writing.");
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
                ESP_LOGW(nameof(Storage), "Storage not initialized.");
            else if (!SPIFFS.remove(_filename))
                ESP_LOGE(nameof(Storage), "Failed to delete config file.");
            
            _mutex.unlock();

            ESP_LOGI(nameof(Storage), "Config reset, restarting...");

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
};

const char* ReadieFur::EspGps::Storage::_filename = "/config.json";
std::list<const char*> ReadieFur::EspGps::Storage::_sensitiveEntries =
{
    nameof(MQTT_PASSWORD),
    nameof(MODEM_PASSWORD)
};

bool ReadieFur::EspGps::Storage::_initialized = false;
JsonDocument ReadieFur::EspGps::Storage::Cache;
std::mutex ReadieFur::EspGps::Storage::_mutex;

#define GetConfig(T, key) ReadieFur::EspGps::Storage::Cache[#key].as<T>()
#define SetConfig(key, value) ReadieFur::EspGps::Storage::Cache[#key] = value;
