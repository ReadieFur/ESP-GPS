#pragma once

#include <map>
#include <variant>
#include <ArduinoJson.h>
#include "GPS.hpp"
#include "Battery.hpp"

class Publish
{
private:
    static bool GetGsmLocation(float& latitude, float& longitude, int& altitude)
    {
        //TODO: Possibly use the gsm_rssi to improve/decrease the radius the discovered value could possibly fall under.
        String gsmLocation = GSM::Modem.getGsmLocation();
        
        int firstComma = gsmLocation.indexOf(',');
        int secondComma = gsmLocation.indexOf(',', firstComma + 1);

        if (firstComma == -1 || secondComma == -1)
            return false;

        latitude = gsmLocation.substring(0, firstComma).toFloat();
        longitude = gsmLocation.substring(firstComma + 1, secondComma).toFloat();
        altitude = gsmLocation.substring(secondComma + 1).toInt();

        return true;
    }

public:
    static void Init() {}

    static bool Loop()
    {
        SerialMon.println("Preparing to send data...");

        std::map<String, std::variant<String, int, uint, bool, long, ulong, double>> data;
        bool sendingEstimatedData = false;

        if (GPS::Gps.location.isUpdated())
        {
            data.insert({"loc_lat", GPS::Gps.location.lat()});
            data.insert({"loc_lng", GPS::Gps.location.lng()});
            data.insert({"loc_quality", GPS::Gps.location.FixQuality()});
            data.insert({"loc_age", GPS::Gps.location.age()});
        }
        else if (GPS::Gps.location.age() > 60 * 1000) //If the GPS location hasn't been updated in x multiple of the update interval, send the estimated location from the modem.
        {
            sendingEstimatedData = true;

            float lat, lng;
            int alt;
            if (!GetGsmLocation(lat, lng, alt))
            {
                SerialMon.println("Failed to send data. Reason: Location invalid.");
                return false;
            }

            data.insert({"loc_lat", lat});
            data.insert({"loc_lng", lng});
            data.insert({"loc_quality", 8}); //See TinyGPSLocation::Quality.
            data.insert({"loc_age", GPS::Gps.location.age()});
            data.insert({"alt", alt});
            data.insert({"alt_age", GPS::Gps.altitude.age()});
        }

        if (GPS::Gps.date.isValid())
            data.insert({"date", GPS::Gps.date.value()});

        if (GPS::Gps.time.isValid())
            data.insert({"time", GPS::Gps.time.value()});

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

        if (!sendingEstimatedData && GPS::Gps.altitude.isValid())
        {
            data.insert({"alt", GPS::Gps.altitude.meters()});
            data.insert({"alt_age", GPS::Gps.altitude.age()});
        }

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

        data.insert({"bat_vlt", Battery::GetVoltage()});
        data.insert({"bat_state", Battery::State});

        // data.insert({"gsm_op", GSM::Modem.getOperator()});
        // data.insert({"gsm_rssi", GSM::Modem.getSignalQuality()});
        // data.insert({"gsm_ip", GSM::Modem.getLocalIP()});

        JsonDocument json;
        for (auto &&kvp : data)
            //std::visit automatically casts the variant type back to it's original data type.
            std::visit([&](auto&& arg) { json[kvp.first] = arg; }, kvp.second);

        #ifdef DUMP_PUBLISH_DATA
        SerialMon.println("Sending data:");
        serializeJsonPretty(json, SerialMon);
        #endif

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
