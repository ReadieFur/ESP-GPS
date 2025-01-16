#pragma once

#include <freertos/FreeRTOS.h>
#include "Service/AService.hpp"
#include "GPS.hpp"
#include "GSM.hpp"
#include <freertos/task.h>
#include "Logging.hpp"
#include <deque>
#include <chrono>
#include <ctime>
#include <mutex>
#include "SLocation.h"
#include <time.h>
#include <Event/AutoResetEvent.hpp>

#define CALCULATE_LOCATION_ON_REQUEST
#define FALLBACK_TO_GSM_ONLY_ON_REQUEST //Reduces network usage but increases the time to get a location by about 2-3 seconds.
#define GSM_ALTERNATIVE_QUERY //Get the location of the cell tower and use local time (in my testing this is more accurate and faster, although has a larger location range).
#define GPS_GET_RAW //Use GPS data as it is requested rather than sampling it.

#ifdef GPS_GET_RAW
#ifndef CALCULATE_LOCATION_ON_REQUEST
#define CALCULATE_LOCATION_ON_REQUEST
#endif
#endif

namespace ReadieFur::EspGps
{
    class Location : public Service::AService
    {
    private:
        EspGps::GPS* _gpsService = nullptr;
        EspGps::GSM* _gsmService = nullptr;
        std::deque<SLocation> _gpsSampleQueue;
        TickType_t _lastGsmSample = 0;
        SLocation _gsmSample;
        #ifdef CALCULATE_LOCATION_ON_REQUEST
        std::mutex _mutex;
        #else
        SLocation _location;
        #endif

        void SampleGPS()
        {
            if (!_gpsService->IsUpdated())
                return;

            SLocation sample;
            _gpsService->GetLocation(sample);

            if (_gpsSampleQueue.size() == 5)
                _gpsSampleQueue.pop_front();
            _gpsSampleQueue.push_back(sample);
        }

        void SampleGSM()
        {
            //Only sample GSM a maximum of once per second.
            if (xTaskGetTickCount() - _lastGsmSample < pdMS_TO_TICKS(1000))
                return;

            SLocation gsmSample;

            _gsmService->QueueAction([this, &gsmSample]()
            {
                std::tm timeInfo = {};
                float lat = 0, lng = 0, acc = 0;

                bool valid = false;
                #ifdef GSM_ALTERNATIVE_QUERY
                TinyGsm* modem = _gsmService->GetModem();

                String locationString = modem->getGsmLocationRaw();
                if (locationString.isEmpty())
                {
                    _lastGsmSample = 0;
                    return;
                }
                //Output: lat,lng,acc
                int comma1 = locationString.indexOf(',');
                int comma2 = locationString.indexOf(',', comma1 + 1);
                if (comma1 == -1 || comma2 == -1)
                {
                    _lastGsmSample = 0;
                    return;
                }
                lat = locationString.substring(0, comma1).toFloat();
                lng = locationString.substring(comma1 + 1, comma2).toFloat();
                acc = locationString.substring(comma2 + 1).toFloat();

                #if true
                //Less accurate but fast.
                float timezone = 0;
                valid = modem->getNetworkTime(
                    &timeInfo.tm_year, &timeInfo.tm_mon, &timeInfo.tm_mday,
                    &timeInfo.tm_hour, &timeInfo.tm_min, &timeInfo.tm_sec, &timezone);
                // //Factor timezone into the timestamp.
                // gsmSample.timestamp = std::mktime(&timeInfo) + (timezone * 3600);
                #else
                //More accurate but slow.
                valid = modem->getGsmLocationTime(
                    &timeInfo.tm_year, &timeInfo.tm_mon, &timeInfo.tm_mday,
                    &timeInfo.tm_hour, &timeInfo.tm_min, &timeInfo.tm_sec);
                #endif
                #else
                valid = _gsmService->GetModem()->getGsmLocation(
                // bool valid = _gsmService->GetLocation(
                    &lat, &lng, &acc,
                    &timeInfo.tm_year, &timeInfo.tm_mon, &timeInfo.tm_mday,
                    &timeInfo.tm_hour, &timeInfo.tm_min, &timeInfo.tm_sec
                    /*, pdMS_TO_TICKS(5000)*/);
                #endif

                if (valid)
                {
                    _lastGsmSample = xTaskGetTickCount();
                    gsmSample.latitude = (double)lat;
                    gsmSample.longitude = (double)lng;
                    gsmSample.accuracy = (double)acc;
                    timeInfo.tm_mon -= 1;
                    timeInfo.tm_year -= 1900;
                    gsmSample.timestamp = std::mktime(&timeInfo);
                }
                else
                {
                    _lastGsmSample = 0;
                }
            }, configIDLE_TASK_STACK_SIZE + 1024);

            _gsmSample = gsmSample;
        }

        void CalculateLocation(SLocation& outLocation, ELocationSource& outSource)
        {
            #ifndef GPS_GET_RAW
            if (!_gpsSampleQueue.empty())
            {
                outSource = ELocationSource::LC_GPS;

                size_t gsmSampleCount = _gpsSampleQueue.size();
                outLocation.latitude = outLocation.longitude = outLocation.accuracy = outLocation.timestamp = 0; //Ensure these are set to 0 as we will be working on them directly.
                size_t timeSampleCount = 0, hdopSampleCount = 0;
                long long timeSamples = 0; //TODO: Change this as in the far future it will encounter the same overflow issue as before when I was using a regular long.
                for (size_t i = 0; i < gsmSampleCount; i++)
                {
                    SLocation gpsSample = _gpsSampleQueue.at(i);
                    outLocation.latitude += gpsSample.latitude;
                    outLocation.longitude += gpsSample.longitude;
                    if (gpsSample.accuracy != 0)
                    {
                        hdopSampleCount++;
                        outLocation.accuracy += gpsSample.accuracy;
                    }
                    if (gpsSample.timestamp != 0)
                    {
                        timeSampleCount++;
                        timeSamples += gpsSample.timestamp;
                    }
                }
                outLocation.latitude /= gsmSampleCount;
                outLocation.longitude /= gsmSampleCount;

                if (hdopSampleCount != 0)
                    outLocation.accuracy /= hdopSampleCount;

                if (timeSampleCount != 0)
                    outLocation.timestamp = (long)(timeSamples / timeSampleCount);
            }
            #else
            if (_gpsService->IsUpdated()) //Ignores sample age.
            {
                outSource = ELocationSource::LC_GPS;
                _gpsService->GetLocation(outLocation);
            }
            #endif
            #ifndef FALLBACK_TO_GSM_ONLY_ON_REQUEST
            else if (_lastGsmSample != 0)
            {
                outSource = ELocationSource::LC_GSM;
                outLocation = _gsmSample;
            }
            else
            {
                outSource = ELocationSource::LC_Invalid;
                return;
            }
            #else
            else
            {
                SampleGSM();
                if (_lastGsmSample != 0)
                {
                    outSource = ELocationSource::LC_GSM;
                    outLocation = _gsmSample;
                }
                else
                {
                    outSource = ELocationSource::LC_Invalid;
                    return;
                }
            }
            #endif

            //Update ESP32 RTC with the obtained time.
            timeval tv = { .tv_sec = outLocation.timestamp, .tv_usec = 0 };
            settimeofday(&tv, nullptr);
        }

    protected:
        void RunServiceImpl() override
        {
            _gpsService = GetService<EspGps::GPS>();
            _gsmService = GetService<EspGps::GSM>();

            TickType_t interval = _gpsService->GetInterval();
            _gsmService->WaitForConnection();

            #if defined(DEBUG) && false
            xTaskCreate([](void* param)
            {
                Location* self = reinterpret_cast<Location*>(param);
                SLocation location;
                while (!self->ServiceCancellationToken.IsCancellationRequested())
                {
                    self->GetLocation(location);
                    LOGD(nameof(Location), "Type:%i, Lat: %.6f, Lng: %.6f, Acc: %.6f, Time: %ld", location.type, location.latitude, location.longitude, location.accuracy, location.timestamp);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }, "location_dbg", configIDLE_TASK_STACK_SIZE + 1024 + 512, this, ServiceEntrypointPriority, nullptr);
            #endif

            TickType_t sampleLifetime;
            #if false
            //Samples should exist for at least 1 second, and 2x the update rate (to avoid discarding samples that are still "valid", due to race conditions in the program).
            sampleLifetime = pdMS_TO_TICKS(pdTICKS_TO_MS(interval) * 2);
            if (sampleLifetime < pdMS_TO_TICKS(1000))
                sampleLifetime = pdMS_TO_TICKS(1000);
            #else
            sampleLifetime = pdMS_TO_TICKS(1000);
            #endif

            #ifndef GPS_GET_RAW
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                TickType_t now = xTaskGetTickCount();
                for (auto &&sample : _gpsSampleQueue)
                {
                    //Only keep samples for x ms.
                    if (now - sample.age > sampleLifetime)
                    {
                        LOGD(nameof(Location), "Removing old GPS sample, age: %ld, diff: %ld, now: %ld, lifetime: %ld", sample.age, now - sample.age, now, sampleLifetime);
                        _gpsSampleQueue.pop_front();
                    }
                }
                #ifdef CALCULATE_LOCATION_ON_REQUEST
                _mutex.lock();
                #endif
                SampleGPS();

                #ifndef FALLBACK_TO_GSM_ONLY_ON_REQUEST
                //Only query the GSM for location data if the GPS has no samples. 
                if (_gpsSampleQueue.empty())
                    SampleGSM();
                #endif

                #ifdef CALCULATE_LOCATION_ON_REQUEST
                _mutex.unlock();
                #else
                SLocation location;
                CalculateLocation(location);
                _location = location;
                #endif

                vTaskDelay(interval);
            }
            #else
            ServiceCancellationToken.WaitForCancellation();
            #endif

            _gpsService = nullptr;
            _gsmService = nullptr;
        }

    public:
        Location()
        {
            ServiceEntrypointStackDepth += 1024;
            #ifndef CALCULATE_LOCATION_ON_REQUEST
            //Some extras space is needed for this.
            //TODO: Figure out how much space I can safely get away with here.
            ServiceEntrypointStackDepth += 256;
            #endif
            AddDependencyType<EspGps::GPS>();
            AddDependencyType<EspGps::GSM>();
            _gpsSampleQueue.resize(5);
        }

        void GetLocation(SLocation& outLocation, ELocationSource& outSource)
        {
            #ifdef CALCULATE_LOCATION_ON_REQUEST
            _mutex.lock();
            CalculateLocation(outLocation, outSource);
            _mutex.unlock();
            #else
            outLocation = _location;
            #endif
        }
    };
};
