#pragma once

#include <Preferences.h>
#include "Helpers.h"
#include "Config.h"

#ifdef BATTERY_DISABLED_PIN
#ifndef BATTERY_UPDATE_INTERVAL
#error "Please define the update interval for when on battery power."
#endif
#define BATTERY_UPDATE_INTERVAL_US BATTERY_UPDATE_INTERVAL * 1000000
#endif

#if SERVER_METHOD < 1 || SERVER_METHOD > 1
#error "Server method not implemented."
#endif

class _ConfigManager : public Preferences
{
public:
    _ConfigManager()
    {
        begin("config", false);

        //Set any new config values from the defaults that may have not already been set.
        ApplyDefaults();
    }

    void RestoreDefaults()
    {
        clear();
        ApplyDefaults();
    }

private:
    void ApplyDefaults()
    {
        //Not everything is included here as some values should remain static and set through the config at compile time.

        if (!isKey(NAMEOF(UPDATE_INTERVAL)))
            putULong(NAMEOF(UPDATE_INTERVAL), UPDATE_INTERVAL);
        if (!isKey(NAMEOF(BATTERY_UPDATE_INTERVAL)))
            putULong(NAMEOF(BATTERY_UPDATE_INTERVAL), BATTERY_UPDATE_INTERVAL);
        if (!isKey(NAMEOF(MAX_RETRIES)))
            putUShort(NAMEOF(MAX_RETRIES), MAX_RETRIES);
        
        if (!isKey(NAMEOF(RESPOND_TO_API_CALLS)))
            putBool(NAMEOF(RESPOND_TO_API_CALLS), RESPOND_TO_API_CALLS);

        if (!isKey(NAMEOF(SERVER_FQDN)))
            putString(NAMEOF(SERVER_FQDN), SERVER_FQDN);
        if (!isKey(NAMEOF(SERVER_PORT)))
            putUShort(NAMEOF(SERVER_PORT), SERVER_PORT);
        if (!isKey(NAMEOF(SERVER_PATH)))
            putString(NAMEOF(SERVER_PATH), SERVER_PATH);
        if (!isKey(NAMEOF(SERVER_SSL)))
            putBool(NAMEOF(SERVER_SSL), SERVER_SSL);
        if (!isKey(NAMEOF(SERVER_METHOD)))
            putUChar(NAMEOF(SERVER_METHOD), SERVER_METHOD);
        if (!isKey(NAMEOF(SERVER_HEADER_CONTENT_TYPE)))
            putString(NAMEOF(SERVER_HEADER_CONTENT_TYPE), SERVER_HEADER_CONTENT_TYPE);
        #ifdef SERVER_HEADER_AUTHORIZATION
            if (!isKey(NAMEOF(SERVER_HEADER_AUTHORIZATION)))
                putString(NAMEOF(SERVER_HEADER_AUTHORIZATION), SERVER_HEADER_AUTHORIZATION);
        #endif
        if (!isKey(NAMEOF(SERVER_DATA_FORMAT)))
            putString(NAMEOF(SERVER_DATA_FORMAT), SERVER_DATA_FORMAT);
    }

    // template<typename T>
    // size_t PutIfNotExists(size_t (*putFunc)(const char*, T), const char* key, T value)
    // {
    //     if (!isKey(key))
    //         return putFunc(key, value);
    //     else
    //         return 0;
    // }
};

_ConfigManager ConfigManager;
