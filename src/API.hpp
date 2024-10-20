#pragma once

#include <WString.h>
#include "Board.h"
#include <map>
#include <functional>
#include "Storage.hpp"
#include <algorithm>
#include "Battery.hpp"

class API
{
private:
    static std::map<String, std::function<uint(String)>> _apiMethods;

    static uint Echo (String data) { SerialMon.printf("Echo: %s\n", data); return 200; }

    static uint Ping(String) { SerialMon.println("pong"); return 200; }

    #pragma region Config
    static uint ConfigSave(String) { return Storage::Save() ? 200 : 500; }

    static uint ConfigSet(String data)
    {
        int spaceIndex = data.indexOf(' ');
        if (spaceIndex == -1)
        {
            SerialMon.println("Command invalid, expected key and value, only got a key.");
            return 400;
        }

        //TODO: Provide type checking?
        String key = data.substring(0, spaceIndex);
        String value = data.substring(spaceIndex + 1);
        Storage::Cache[key] = value;
        //Don't call the save method here, it's faster to call save manually ad the end of all set calls if multiple values are to be written.
        // Storage::Save();

        SerialMon.printf("Set '%s' to '%s'.\n", key, value);
        return 200;
    }

    static uint ConfigGet(String data)
    {
        if (Storage::Cache[data].isNull())
        {
            SerialMon.printf("Key '%s' does not exist.\n", data);
            return 404;
        }

        if (Storage::IsSensitive(data.c_str()))
        {
            //TODO: Provide authentication to allow access to sensitive values.
            SerialMon.printf("Access to sensitive key '%s' is forbidden via API calls.\n", data);
            return 403;
        }

        String value = Storage::Cache[data].as<String>();
        SerialMon.printf("The value for key '%s' is '%s'.\n", data, value);

        return 200;
    }

    static uint ConfigReset(String) { Storage::ResetAllConfig(); return 200; }
    #pragma endregion

    static uint BatteryCheck(String)
    {
        uint32_t voltage;
        Battery::EState state;
        Battery::UpdateVoltage(&voltage, &state);

        String stateString;
        switch (state)
        {
        case Battery::EState::Charging:
            stateString = "charging";
            break;
        case Battery::EState::Discharging:
            stateString = "discharging";
            break;
        case Battery::EState::Discharging_Low:
            stateString = "low";
            break;
        case Battery::EState::Discharging_Critical:
            stateString = "critical";
            break;
        }

        SerialMon.printf("The battery voltage is %imV and is currently %s.\n", voltage, stateString);

        return 200;
    }

    static uint Reboot(String) { ESP.restart(); return 202; } //Does not return.

public:
    static void Init() {}

    static void Loop() {}

    //TODO: Provide a stream to send the response strings to.
    static uint ProcessMessage(String message)
    {
        message.trim();

        for (auto &&kvp : _apiMethods)
        {
            if (!message.startsWith(kvp.first))
                continue;

            // kvp.second(message);
            String data = message.substring(kvp.first.length());
            data.trim();
            return kvp.second(data);
        }

        SerialMon.printf("No matching API call found for '%s'.\n", message);
        return 404;
    }
};

std::map<String, std::function<uint(String)>> API::_apiMethods =
{
    { "echo", API::Echo },
    { "ping", API::Ping },
    { "config save", API::ConfigSave },
    { "config set", API::ConfigSet },
    { "config get", API::ConfigGet },
    { "config reset", API::ConfigReset },
    { "battery check", API::BatteryCheck },
    { "reboot", API::Reboot }
};
