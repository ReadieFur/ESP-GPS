#pragma once

#include <map>
#include <variant>
#include <ArduinoJson.h>
#include "Location.hpp"
#include "Battery.hpp"
#include <chrono>
#include <ctime>
#include <esp_sleep.h>

class Publish
{
public:
    static void Init() {}

    static bool Loop()
    {
        SerialMon.println("Sending data...");

        std::map<String, std::variant<String, int, uint, bool, long, ulong, double>> data;
        bool sendingEstimatedData = false;

        //TODO: Send my own accuracy result codes.

        Location::SLocation location;
        if (Location::GetLocation(location))
        {
            //TODO: Sample data and send a more accurate result.
            data.insert({"loc_lat", location.latitude});
            data.insert({"loc_lng", location.longitude});
            // data.insert({"loc_alt", location.altitude});
            data.insert({"loc_confidence", location.confidence});
            data.insert({"loc_type", location.type});
            data.insert({"timestamp", location.timestamp});
            // data.insert({"loc_age", GPS::Gps.location.age()});
        }
        else
        {
            SerialMon.println("Failed to send data. Reason: GPS location not updated.");
            return false;
        }

        // if (GPS::Gps.speed.isValid())
        // {
        //     data.insert({"mps", GPS::Gps.speed.mps()});
        //     data.insert({"mps_age", GPS::Gps.speed.age()});
        // }

        // if (GPS::Gps.course.isValid())
        // {
        //     data.insert({"deg", GPS::Gps.course.deg())});
        //     data.insert({"deg_age", GPS::Gps.course.age()});
        // }

        // if (!sendingEstimatedData && GPS::Gps.altitude.isValid())
        // {
        //     data.insert({"alt", GPS::Gps.altitude.meters()});
        //     data.insert({"alt_age", GPS::Gps.altitude.age()});
        // }

        // if (GPS::Gps.satellites.isValid())
        // {
        //     data.insert({"sat", GPS::Gps.satellites.value()});
        //     data.insert({"sat_age", GPS::Gps.satellites.age()});
        // }

        // if (GPS::Gps.hdop.isValid())
        // {
        //     data.insert({"hdop", GPS::Gps.hdop.hdop()});
        //     data.insert({"hdop_age", GPS::Gps.satellites.age()});
        // }

        uint32_t batteryVoltageMv;
        Battery::EState batteryState;
        Battery::GetStatus(&batteryVoltageMv, &batteryState);
        data.insert({"bat_vlt", batteryVoltageMv});
        data.insert({"bat_state", batteryState});
 
        data.insert({"trigger", esp_sleep_get_wakeup_cause()});

        // data.insert({"gsm_op", GSM::Modem.getOperator()});
        // data.insert({"gsm_rssi", GSM::Modem.getSignalQuality()});
        // data.insert({"gsm_ip", GSM::Modem.getLocalIP()});

        JsonDocument json;
        for (auto &&kvp : data)
            //std::visit automatically casts the variant type back to it's original data type.
            std::visit([&](auto&& arg) { json[kvp.first] = arg; }, kvp.second);

        String mqttPayload;
        serializeJson(json, mqttPayload);
        if (!MQTT::Mqtt.publish(MQTT::PublishTopic, mqttPayload.c_str()))
        {
            SerialMon.print("Failed to send MQTT message: ");
            SerialMon.println(MQTT::Mqtt.state());
            return false;
        }

        return true;
    }
};
