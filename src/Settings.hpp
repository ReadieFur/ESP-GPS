#pragma once

#include "Helpers.h"
#include "Config.h"
#include <Preferences.h>
#include <map>
#include <typeinfo>
#include <tuple>
#include <any>
#include <variant>

//https://community.platformio.org/t/use-c-17-or-newer-by-default-for-framework-arduino/31978
#define _mapKey(setting) { #setting, std::make_tuple(typeid(setting).hash_code(), setting) }

// typedef std::variant<bool, int8_t, double_t, float_t, int32_t, int64_t, int32_t, int16_t, String, const char*, uint8_t, uint32_t, uint64_t, uint32_t, uint16_t> PreferenceType;

class Settings
{
private:
    Preferences _preferences;

    std::map<const char*, std::tuple<std::size_t, std::any>> _keyTypeMap =
    {
        _mapKey(MODEM_RST),
        _mapKey(UPDATE_INTERVAL),
        _mapKey(BATTERY_UPDATE_INTERVAL),
        _mapKey(MAX_RETRIES),
        _mapKey(RETRY_INTERVAL),
        _mapKey(RESPOND_TO_API_CALLS),
        _mapKey(SERVER_FQDN),
        _mapKey(SERVER_PORT),
        _mapKey(SERVER_PATH),
        _mapKey(SERVER_SSL),
        _mapKey(SERVER_METHOD),
        _mapKey(SERVER_HEADER_CONTENT_TYPE),
        _mapKey(SERVER_HEADER_AUTHORIZATION),
        _mapKey(SERVER_DATA_FORMAT),
        _mapKey(SERVER_ADDITIONAL_HEADERS),
    };

    static const char* GetPreferencesKey(const char* longKey)
    {
        //Take every first letter after _.
        String key = String(longKey[0]);
        for (size_t i = 1; i < strlen(longKey); i++)
        {
            if (longKey[i] == '_')
                key += longKey[i + 1];
        }
        return key.c_str();
    }

public:
    Settings()
    {
        _preferences.begin("config", false);

        #if defined(RESTORE_DEFAULTS_ON_FLASH) || (defined(_DEBUG) && true)
        _preferences.clear();
        #endif

        for (auto &&kvp : _keyTypeMap)
        {
            const char* shortKey = GetPreferencesKey(kvp.first);
            if (!_preferences.isKey(shortKey))
                Set(shortKey, std::get<1>(kvp.second));
        }
    }

    ~Settings()
    {
        _preferences.end();
    }

    template<typename T>
    T Get(const char* key)
    {
        auto kvp = _keyTypeMap.find(key);
        if (kvp == _keyTypeMap.end())
            return 0;

        std::size_t mapType = std::get<0>(kvp->second);
        T defaultValue = std::any_cast<T>(std::get<1>(kvp->second));

        if (typeid(T).hash_code() != mapType)
            return 0;

        const char* shortKey = GetPreferencesKey(key);

        if constexpr (std::is_same<T, bool>::defaultValue)
            return _preferences.getBool(shortKey, defaultValue);
        else if constexpr (std::is_same<T, int8_t>::defaultValue)
            return _preferences.getChar(shortKey, defaultValue);
        else if constexpr (std::is_same<T, double>::defaultValue)
            return _preferences.getDouble(shortKey, defaultValue);
        else if constexpr (std::is_same<T, float>::defaultValue)
            return _preferences.getFloat(shortKey, defaultValue);
        else if constexpr (std::is_same<T, int32_t>::defaultValue)
            return _preferences.getInt(shortKey, defaultValue);
        else if constexpr (std::is_same<T, int64_t>::defaultValue)
            return _preferences.getLong64(shortKey, defaultValue);
        else if constexpr (std::is_same<T, int16_t>::defaultValue)
            return _preferences.getShort(shortKey, defaultValue);
        else if constexpr (std::is_same<T, std::string>::defaultValue)
            return _preferences.getString(shortKey, defaultValue);
        else if constexpr (std::is_same<T, const char*>::defaultValue)
            return _preferences.getString(shortKey, defaultValue);
        else if constexpr (std::is_same<T, uint8_t>::defaultValue)
            return _preferences.getUChar(shortKey, defaultValue);
        else if constexpr (std::is_same<T, uint32_t>::defaultValue)
            return _preferences.getUInt(shortKey, defaultValue);
        else if constexpr (std::is_same<T, uint64_t>::defaultValue)
            return _preferences.getULong64(shortKey, defaultValue);
        else if constexpr (std::is_same<T, uint16_t>::defaultValue)
            return _preferences.getUShort(shortKey, defaultValue);
        else
            return T(); // Unsupported type
    }

    template<typename T>
    size_t Set(const char* key, T value)
    {
        auto kvp = _keyTypeMap.find(key);
        if (kvp == _keyTypeMap.end())
            return 0;

        std::size_t expectedType = std::get<0>(kvp->second);
        if (typeid(T).hash_code() != expectedType)
            return 0;

        const char* shortKey = GetPreferencesKey(key);

        if constexpr (std::is_same<T, bool>::value)
            return _preferences.putBool(shortKey, value);
        else if constexpr (std::is_same<T, int8_t>::value)
            return _preferences.putChar(shortKey, value);
        else if constexpr (std::is_same<T, double>::value)
            return _preferences.putDouble(shortKey, value);
        else if constexpr (std::is_same<T, float>::value)
            return _preferences.putFloat(shortKey, value);
        else if constexpr (std::is_same<T, int32_t>::value)
            return _preferences.putInt(shortKey, value);
        else if constexpr (std::is_same<T, int64_t>::value)
            return _preferences.putLong64(shortKey, value);
        else if constexpr (std::is_same<T, int16_t>::value)
            return _preferences.putShort(shortKey, value);
        else if constexpr (std::is_same<T, std::string>::value)
            return _preferences.putString(shortKey, value);
        else if constexpr (std::is_same<T, const char*>::value)
            return _preferences.putString(shortKey, value);
        else if constexpr (std::is_same<T, uint8_t>::value)
            return _preferences.putUChar(shortKey, value);
        else if constexpr (std::is_same<T, uint32_t>::value)
            return _preferences.putUInt(shortKey, value);
        else if constexpr (std::is_same<T, uint64_t>::value)
            return _preferences.putULong64(shortKey, value);
        else if constexpr (std::is_same<T, uint16_t>::value)
            return _preferences.putUShort(shortKey, value);
        else
            return 0; // Unsupported type
    }
};

Settings settings;

#undef _mapKey
