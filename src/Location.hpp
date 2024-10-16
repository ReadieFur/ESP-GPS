#pragma once

#include "GPS.hpp"
#include "GSM.hpp"
#include <freertos/task.h>
#include "Board.h"

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

private:
    static const size_t DESIRED_SAMPLES = 20;
    static const int SCAN_INTERVAL = 50;
    static double _lat, _lng;
    static float _confidence;
    static ELocationType _locationType;

public:
    static void Init() {}

    static bool Loop()
    {
        //Attempt to get x number of GPS samples.
        /** Not initializing these to 0 seems to cause errors on negative positions.
         * My assumption is because the += operation on an uninitialized value with a negative number causes it to underflow?
         * But that doesn't make sense as the error I was seeing looked like the averages kept increasing every iteration of the loop, e.g. iter1 = 0.7, iter2 = 0.14 etc.
         */
        double totalLat = 0, totalLng = 0;
        int acquiredSamples = 0;
        for (int i = 0; i < DESIRED_SAMPLES; i++)
        {
            GPS::Loop();

            if (GPS::Gps.location.isUpdated())
            {
                totalLat += GPS::Gps.location.lat();
                totalLng += GPS::Gps.location.lng();
                SerialMon.printf("GPS sample %i: %f,%f\n", acquiredSamples + 1, GPS::Gps.location.lat(), GPS::Gps.location.lng());
                // samples.push_back(GPS::Gps.satellites.value());
                acquiredSamples++;
            }

            vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL));
        }

        SerialMon.printf("Acquired %i GPS samples. Target was %i samples.\n", acquiredSamples, DESIRED_SAMPLES);
        if (acquiredSamples > 0)
        {
            _lat = totalLat / (double)acquiredSamples;
            _lng = totalLng / (double)acquiredSamples;

            /** GPS confidence in this program is determined by:
             * --Number of satellite fixes--
             * Number of samples acquired
             * 
             * Through my testing when a high number of samples are acquired the accuracy is relatively high,
             * however I will leave this to be customized on the API end.
             * 
             * Instead here for now I will provide an arbitrary confidence value between 0 and 1.
             */
            _confidence = (float)acquiredSamples / DESIRED_SAMPLES;
            _locationType = ELocationType::GPS;

            SerialMon.printf("Average of GPS samples: %f,%f\n", _lat, _lng);

            return true;
        }

        //Fall back to GSM location.
        SerialMon.println("Falling back to GSM location.");
        float gsmLat, gsmLng;
        if (GSM::Modem.getGsmLocation(&gsmLat, &gsmLng))
        {
            _lat = gsmLat;
            _lng = gsmLng;

            //When we use the GSM location the accuracy is often pretty low, so I will set an arbitrary value of 10%.
            _confidence = 0.1f;
            _locationType = ELocationType::GSM;

            return true;
        }

        _confidence = -1;
        _locationType = ELocationType::Invalid;
        return false;
    }

    static bool GetLocation(double& latitude, double& longitude, float& confidence, ELocationType& locationType)
    {
        latitude = _lat;
        longitude = _lng;
        confidence = _confidence;
        locationType = _locationType;
        return locationType != ELocationType::Invalid;
    }
};

double Location::_lat = 0;
double Location::_lng = 0;
float Location::_confidence = -1;
Location::ELocationType Location::_locationType = Location::ELocationType::Invalid;
