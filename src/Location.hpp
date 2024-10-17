#pragma once

#include "GPS.hpp"
#include "GSM.hpp"
#include <freertos/task.h>
#include "Board.h"
#include <chrono>
#include <ctime>

//TODO: Have this constantly run on it's own thread with an always updating location buffer.
class Location
{
public:
    enum ELocationType
    {
        Invalid,
        GPS,
        GSM
    };

    struct SLocation
    {
        double latitude = 0, longitude = 0, altitude = 0, confidence = -1;
        time_t timestamp = 0;
        ELocationType type = ELocationType::Invalid;
    };

private:
    static const size_t DESIRED_SAMPLES = 20;
    static const int SCAN_INTERVAL = 50;
    static SLocation _location;

public:
    static void Init() {}

    static bool Loop()
    {
        //Attempt to get x number of GPS samples.
        /** Not initializing these to 0 seems to cause errors on negative positions.
         * My assumption is because the += operation on an uninitialized value with a negative number causes it to underflow?
         * But that doesn't make sense as the error I was seeing looked like the averages kept increasing every iteration of the loop, e.g. iter1 = 0.7, iter2 = 0.14 etc.
         */
        double totalLat = 0, totalLng = 0, totalAlt = 0, acquiredSamples = 0;
        for (size_t i = 0; i < DESIRED_SAMPLES; i++)
        {
            GPS::Loop();

            if (GPS::Gps.location.isUpdated())
            {
                totalLat += GPS::Gps.location.lat();
                totalLng += GPS::Gps.location.lng();
                totalAlt += GPS::Gps.altitude.meters();
                SerialMon.printf("GPS sample %i: %f,%f\n", acquiredSamples + 1, GPS::Gps.location.lat(), GPS::Gps.location.lng());
                // samples.push_back(GPS::Gps.satellites.value());
                acquiredSamples++;
            }

            vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL));
        }

        SerialMon.printf("Acquired %i GPS samples. Target was %i samples.\n", acquiredSamples, DESIRED_SAMPLES);
        if (acquiredSamples > 0)
        {
            //I previously just sent the raw date and time to reduce processing time on the device, however the power draw is minimal so I will process a timestamp on this device instead.
            std::tm timeInfo =
            {
                tm_sec: GPS::Gps.time.second(),
                tm_min: GPS::Gps.time.minute(),
                tm_hour: GPS::Gps.time.hour(),
                tm_mday: GPS::Gps.date.day(),
                tm_mon: GPS::Gps.date.month() - 1,
                tm_year: GPS::Gps.date.year() - 1900
            };
            _location.timestamp = std::mktime(&timeInfo);

            _location.latitude = totalLat / acquiredSamples;
            _location.longitude = totalLng / acquiredSamples;
            _location.altitude = totalAlt / acquiredSamples;

            /** GPS confidence in this program is determined by:
             * --Number of satellite fixes--
             * Number of samples acquired
             * 
             * Through my testing when a high number of samples are acquired the accuracy is relatively high.
             * It seems to be that 10 samples can be acquired per second if the GPS has a good fix.
             * I will use the number of samples acquired as the confidence level.
             * With a good fix the accuracy seems to be around 10m.
             * In the future I will use the task based proposal to get a more accurate confidence level.
             */
            //TODO: Change this arbitrary accuracy system.
            _location.confidence = acquiredSamples >= 10 ? 10 : 100 - acquiredSamples * 10 + 10;
            _location.type = ELocationType::GPS;

            SerialMon.printf("Average of GPS samples: %f,%f\n", _location.latitude, _location.longitude);

            return true;
        }

        //Fall back to GSM location.
        SerialMon.println("Falling back to GSM location.");
        float gsmLat, gsmLng, gsmAccuracy;
        int gsmYear, gsmMonth, gsmDay, gsmHour, gsmMinute, gsmSecond;
        if (GSM::Modem.getGsmLocation(
            &gsmLat, &gsmLng, &gsmAccuracy,
            &gsmYear, &gsmMonth, &gsmDay,
            &gsmHour, &gsmMinute, &gsmSecond))
        {
            std::tm timeInfo =
            {
                tm_sec: gsmSecond,
                tm_min: gsmMinute,
                tm_hour: gsmHour,
                tm_mday: gsmDay,
                tm_mon: gsmMonth - 1,
                tm_year: gsmYear - 1900
            };
            _location.timestamp = std::mktime(&timeInfo);

            _location.latitude = gsmLat;
            _location.longitude = gsmLng;

            //When we use the GSM location the accuracy is often pretty low.
            _location.confidence = gsmAccuracy;
            _location.type = ELocationType::GSM;

            return true;
        }

        return false;
    }

    static bool GetLocation(SLocation& outLocation)
    {
        outLocation = _location;
        return _location.type != ELocationType::Invalid;
    }
};

Location::SLocation Location::_location = {};
