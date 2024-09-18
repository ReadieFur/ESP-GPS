#pragma once

#include <map>
#include "GPS.hpp"
#include "Battery.hpp"

class Publish
{
private:
    static bool GetGsmLocation(float& latitude, float& longitude, int& altitude)
    {
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

        std::map<String, String> data;
        bool sendingEstimatedData = false;

        if (GPS::Gps.location.isUpdated())
        {
            data.insert({"loc_lat", String(GPS::Gps.location.lat())});
            data.insert({"loc_lng", String(GPS::Gps.location.lng())});
            data.insert({"loc_quality", String(GPS::Gps.location.FixQuality())});
            data.insert({"loc_age", String(GPS::Gps.location.age())});
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

            data.insert({"loc_lat", String(lat)});
            data.insert({"loc_lng", String(lng)});
            data.insert({"loc_quality", String('6')}); //6 means estimated location.
            data.insert({"loc_age", String(GPS::Gps.location.age())});
            data.insert({"alt", String(alt)});
            data.insert({"alt_age", String(GPS::Gps.altitude.age())});
        }

        if (GPS::Gps.date.isValid())
            data.insert({"date", String(GPS::Gps.date.value())});

        if (GPS::Gps.time.isValid())
            data.insert({"time", String(GPS::Gps.time.value())});

        // if (GPS::Gps.speed.isValid())
        // {
        //     data.insert({"mps", String(GPS::Gps.speed.mps())});
        //     data.insert({"mps_age", String(GPS::Gps.speed.age())});
        // }

        // if (GPS::Gps.course.isValid())
        // {
        //     data.insert({"deg", String(GPS::Gps.course.deg())});
        //     data.insert({"deg_age", String(GPS::Gps.course.age())});
        // }

        if (!sendingEstimatedData && GPS::Gps.altitude.isValid())
        {
            data.insert({"alt", String(GPS::Gps.altitude.meters())});
            data.insert({"alt_age", String(GPS::Gps.altitude.age())});
        }

        // if (GPS::Gps.satellites.isValid())
        // {
        //     data.insert({"sat", String(GPS::Gps.satellites.value())});
        //     data.insert({"sat_age", String(GPS::Gps.satellites.age())});
        // }

        // if (GPS::Gps.hdop.isValid())
        // {
        //     data.insert({"hdop", String(GPS::Gps.hdop.hdop())});
        //     data.insert({"hdop_age", String(GPS::Gps.satellites.age())});
        // }

        data.insert({"bat_vlt", String(Battery::GetVoltage())});
        data.insert({"bat_state", String(Battery::State)});

        // data.insert({"gsm_op", GSM::Modem.getOperator()});
        // data.insert({"gsm_rssi", String(GSM::Modem.getSignalQuality())});
        // data.insert({"gsm_ip", GSM::Modem.getLocalIP()});

        String message;
        for (auto &&kvp : data)
        {
            message += kvp.first;
            message += '=';
            message += kvp.second;
            message += '\n';
        }

        #ifdef DUMP_PUBLISH_DATA
        SerialMon.println("Sending data:");
        SerialMon.print(message.c_str());
        #endif

        if (!MQTT::Mqtt.publish(MQTT::PublishTopic, message.c_str()))
        {
            SerialMon.print("Failed to send MQTT message: ");
            SerialMon.println(MQTT::Mqtt.state());
            return false;
        }

        return true;
    }
};
