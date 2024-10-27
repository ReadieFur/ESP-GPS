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

#define CALCULATE_LOCATION_ON_REQUEST

namespace ReadieFur::EspGps
{
    class Location : public Service::AService
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
            ELocationType type = ELocationType::Invalid;
            time_t timestamp = 0;
            double latitude = 0, longitude = 0, accuracy = 0;
        };

    private:
        struct SGPSSample
        {
            time_t timestamp = 0;
            TinyGPSLocation location = {};
            double hdop = 0;
        };

        static const size_t DESIRED_SAMPLES = 20;
        static const int SCAN_INTERVAL = 50;
        EspGps::GPS* _gpsService = nullptr;
        EspGps::GSM* _gsmService = nullptr;
        std::deque<SGPSSample> _gpsSampleQueue;
        SLocation _gsmSample;
        #ifdef CALCULATE_LOCATION_ON_REQUEST
        std::mutex _mutex;
        #else
        SLocation _location;
        #endif

        void SampleGPS()
        {
            if (!_gpsService->TinyGps.location.isUpdated())
                return;

            SGPSSample sample =
            {
                .location = _gpsService->TinyGps.location
            };

            if (_gpsService->TinyGps.date.isValid() && _gpsService->TinyGps.time.isValid())
            {
                std::tm timeInfo =
                {
                    tm_sec: _gpsService->TinyGps.time.second(),
                    tm_min: _gpsService->TinyGps.time.minute(),
                    tm_hour: _gpsService->TinyGps.time.hour(),
                    tm_mday: _gpsService->TinyGps.date.day(),
                    tm_mon: _gpsService->TinyGps.date.month() - 1,
                    tm_year: _gpsService->TinyGps.date.year() - 1900
                };

                sample.timestamp = std::mktime(&timeInfo);
                //age = timestamp + (timestamp.age - location.age)
                int32_t timeOffset = _gpsService->TinyGps.time.age() - _gpsService->TinyGps.location.age();
                time_t secondsToAdd = timeOffset / 1000;
                sample.timestamp += secondsToAdd;
            }

            if (_gpsService->TinyGps.hdop.isValid())
                sample.hdop = _gpsService->TinyGps.hdop.hdop();

            if (_gpsSampleQueue.size() == 5)
                _gpsSampleQueue.pop_front();
            _gpsSampleQueue.push_back(sample);
        }

        void SampleGSM()
        {
            SLocation gsmSample;

            _gsmService->QueueAction([this, &gsmSample]()
            {
                std::tm timeInfo = {};
                float lat = 0, lng = 0, acc = 0;

                bool valid = _gsmService->GetModem()->getGsmLocation(
                // bool valid = _gsmService->GetLocation(
                    &lat, &lng, &acc,
                    &timeInfo.tm_year, &timeInfo.tm_mon, &timeInfo.tm_mday,
                    &timeInfo.tm_hour, &timeInfo.tm_min, &timeInfo.tm_sec
                    /*, pdMS_TO_TICKS(5000)*/);

                if (valid)
                {
                    gsmSample.type = ELocationType::GSM;
                    gsmSample.latitude = (double)lat;
                    gsmSample.longitude = (double)lng;
                    gsmSample.accuracy = (double)acc;
                    timeInfo.tm_mon -= 1;
                    timeInfo.tm_year -= 1900;
                    gsmSample.timestamp = std::mktime(&timeInfo);
                }
            }, configIDLE_TASK_STACK_SIZE + 1024);

            _gsmSample = gsmSample;
        }

        void CalculateLocation(SLocation& outLocation)
        {
            if (!_gpsSampleQueue.empty())
            {
                outLocation.type = ELocationType::GPS;

                size_t gsmSampleCount = _gpsSampleQueue.size();
                outLocation.latitude = outLocation.longitude = outLocation.accuracy = outLocation.timestamp = 0; //Ensure these are set to 0 as we will be working on them directly.
                size_t timeSampleCount = 0, hdopSampleCount = 0;
                long long timeSamples = 0; //TODO: Change this as in the far future it will encounter the same overflow issue as before when I was using a regular long.
                for (size_t i = 0; i < gsmSampleCount; i++)
                {
                    SGPSSample gpsSample = _gpsSampleQueue.at(i);
                    outLocation.latitude += gpsSample.location.lat();
                    outLocation.longitude += gpsSample.location.lng();
                    if (gpsSample.hdop != 0)
                    {
                        hdopSampleCount++;
                        outLocation.accuracy += gpsSample.hdop;
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
                {
                    outLocation.accuracy /= hdopSampleCount;
                    outLocation.accuracy *= 5.0; //Common multiplier for HDOP to meters.
                }

                if (timeSampleCount != 0)
                {
                    outLocation.timestamp = (long)(timeSamples / timeSampleCount);
                }
            }
            else if (_gsmSample.type != ELocationType::Invalid)
            {
                outLocation = _gsmSample;
            }
            else
            {
                outLocation.type = ELocationType::Invalid;
            }
        }

    protected:
        void RunServiceImpl() override
        {
            _gpsService = GetService<EspGps::GPS>();
            _gsmService = GetService<EspGps::GSM>();

            _gsmService->WaitForConnection();

            #if defined(DEBUG) && true
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
            }, "location_dbg", configIDLE_TASK_STACK_SIZE + 1024, this, ServiceEntrypointPriority, nullptr);
            #endif

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                for (auto &&sample : _gpsSampleQueue)
                {
                    //Only keep samples for 1 second.
                    if (sample.location.age() > 1000)
                        _gpsSampleQueue.pop_front();
                }
                #ifdef CALCULATE_LOCATION_ON_REQUEST
                _mutex.lock();
                #endif
                SampleGPS();

                //Only query the GSM for location data if the GPS has no samples. 
                if (_gpsSampleQueue.empty())
                    SampleGSM();

                #ifdef CALCULATE_LOCATION_ON_REQUEST
                _mutex.unlock();
                #else
                SLocation location;
                CalculateLocation(location);
                _location = location;
                #endif

                vTaskDelay(pdMS_TO_TICKS(1000 / 5));
            }

            _gpsService = nullptr;
            _gsmService = nullptr;
        }

    public:
        Location()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<EspGps::GPS>();
            AddDependencyType<EspGps::GSM>();
            _gpsSampleQueue.resize(5);
        }

        void GetLocation(SLocation& outLocation)
        {
            #ifdef CALCULATE_LOCATION_ON_REQUEST
            _mutex.lock();
            CalculateLocation(outLocation);
            _mutex.unlock();
            #else
            outLocation = _location;
            #endif
        }
    };
};
