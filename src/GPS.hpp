#pragma once

#include <freertos/FreeRTOS.h>
#include "Board.h"
#include "Config.h"
#include "Service/AService.hpp"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "Logging.hpp"
#include "Helpers.h"
#include "SLocation.h"
#include <freertos/task.h>
#include <esp_timer.h>
#include <Event/ManualResetEvent.hpp>

#if !defined(GPS_RX) || !defined(GPS_TX)
#define GPS_INTEGRATED
#include "GSM.hpp"
#endif

#define US_TO_S(x) (x / 1000000.0)

namespace ReadieFur::EspGps
{
    class GPS : public Service::AService
    {
    private:
        TinyGPSPlus _tinyGps;
        Event::ManualResetEvent _locationUpdatedEvent;
        SLocation _location;
        TickType_t _interval = pdMS_TO_TICKS(1000);
        std::mutex _locationMutex;
        #ifdef GPS_INTEGRATED
        TinyGsm* _modem;
        std::mutex* _modemMutex;
        // TaskHandle_t _secondaryTaskHandle = nullptr;
        #endif
        #ifdef BATTERY_ADC
        Battery* _batteryService;
        #endif
        uint16_t _predictedTimeToFirstFix = 120; //Assume cold start.

        void SetupGPIO()
        {
            #ifdef GPS_PPS
            pinMode(GPS_PPS, INPUT_PULLDOWN);
            #endif
            #ifdef GPS_WAKEUP
            pinMode(GPS_PPS, INPUT_PULLDOWN);
            #endif
            #ifndef GPS_INTEGRATED
            pinMode(GPS_RX, INPUT_PULLDOWN);
            pinMode(GPS_TX, OUTPUT);
            #endif
        }

        void PowerOn()
        {
            #ifndef GPS_INTEGRATED
            GPS_UART.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
            #ifdef GPS_WAKEUP
            gpio_hold_dis((gpio_num_t)GPS_WAKEUP);
            digitalWrite(GPS_WAKEUP, !GPS_SLEEP_LEVEL);
            //Going based off of the NEO-6M which has a frequency of 5Hz.
            //I should probably scan faster than this however not much data is output so the Rx buffer shouldn't get full.
            _interval = pdMS_TO_TICKS(1000 / 5);
            #endif
            _predictedTimeToFirstFix = 120;
            #else
            _modemMutex->lock();
            //https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX/blob/main/examples/GPS_Acceleration/GPS_Acceleration.ino
            //Validate the module for GPS support.
            String modemName = "UNKOWN";
            modemName = _modem->getModemName();
            if (modemName == "UNKOWN")
            {
                LOGE(nameof(GPS), "Unable to obtain module information.");
                abort();
            }
            else if (modemName.startsWith("A7670G"))
            {
                LOGE(nameof(GPS), "A7670G does not support built-in GPS function.");
                abort();
            }

            //Not sure what this command does.
            _modem->sendAT("+SIMCOMATI");
            _modem->waitResponse();

            //Enable GPS.
            //isEnableGPS is invalid for my module so I have to manually check.
            bool isGpsEnabled = false;
            _modem->sendAT("+CGNSSPWR?");
            if (_modem->waitResponse("+CGNSSPWR:") == 1)
            {
                // +CGNSSPWR: <GNSS_Power_status>
                String gnssPwrRes = _modem->stream.readStringUntil('\n');
                gnssPwrRes.trim();
                //Get the first character that is a digit.
                for (char c : gnssPwrRes)
                {
                    if (isdigit(c))
                    {
                        isGpsEnabled = c == '1';
                        break;
                    }
                }
            }

            //Turn the GPS module on if it is not already on.
            if (!isGpsEnabled)
            {
                LOGI(nameof(GPS), "Powering on GPS...");
                TickType_t start = xTaskGetTickCount();
                while (!_modem->enableGPS())
                {
                    if (xTaskGetTickCount() - start > pdMS_TO_TICKS(15 * 1000)) //Documentation says this should take about 9 seconds.
                    {
                        LOGE(nameof(GPS), "Failed to enable GPS.");
                        abort();
                    }
                }

                //Attempt to set the GPS mode to 7 (GPS + BDS + GLONASS), if this fails the leave it at the module default.
                _modem->setGPSMode(7);

                //Dynamically pick between using cold, warm and hot start.
                double timeSinceBoot = US_TO_S(esp_timer_get_time());
                /* If the module has been off for more than 10 minutes then do a cold start, average TTFF 30-120s.
                 * If the module has been off for less than 10 minutes but more than 30 seconds then do a warm start, average TTFF 15-45.
                 * If the module has been off for less than 30 seconds then do a hot start, average TTFF 1-10s.
                 */
                if (timeSinceBoot > 10 * 60)
                {
                    _modem->sendAT("+CGPSCOLD");
                    _predictedTimeToFirstFix = 120;
                }
                else if (timeSinceBoot > 30)
                {
                    _modem->sendAT("+CGPSWARM");
                    _predictedTimeToFirstFix = 45;
                }
                else
                {
                    _modem->sendAT("+CGPSHOT");
                    _predictedTimeToFirstFix = 10;
                }
            }

            _modem->setGPSBaud(115200);
            #ifdef DEBUG
            // _modem->sendAT("+CGNSSPORTSWITCH=1,1"); //Send NEMA output to UART interface.
            _modem->sendAT("+CGNSSTST=1"); //Enable NMEA output (defaults to USB interface).
            #endif

            //Get update frequency.
            _modem->sendAT("+CGPSNMEARATE?");
            if (_modem->waitResponse("+CGPSNMEARATE:") == 1)
            {
                String nmeaRateRes = _modem->stream.readStringUntil('\n');
                nmeaRateRes.trim();
                _interval = pdMS_TO_TICKS(1000 / nmeaRateRes.toInt());
            }
            //Else leave at default of 1 update per second.

            // if (!_modem->enableAGPS())
            //     LOGW(nameof(GPS), "Failed to enable AGPS.");
            //enableAGPS calls isEnableGPS internally which is invalid for my module, it should be enabled by this point anyway.
            _modem->sendAT("+CAGPS");
            if (_modem->waitResponse(30000UL, "+AGPS:") != 1)
            {
                LOGW(nameof(GPS), "Failed to enable AGPS.");
            }
            else
            {
                String enableAgpsRes = _modem->stream.readStringUntil('\n');
                if (!enableAgpsRes.startsWith(" success."))
                {
                    LOGW(nameof(GPS), "Failed to enable AGPS.");
                }
                else
                {
                    LOGD(nameof(GPS), "AGPS enabled.");
                }
            }

            _modemMutex->unlock();
            #endif
        }

        void PowerOff()
        {
            #ifndef GPS_INTEGRATED
            GPS_UART.end();
            #ifdef GPS_WAKEUP
            digitalWrite(GPS_WAKEUP, GPS_SLEEP_LEVEL);
            gpio_hold_en((gpio_num_t)GPS_WAKEUP);
            #endif
            #else
            _modemMutex->lock();
            _modem->disableGPS();
            _modemMutex->unlock();
            #endif
        }

        void ParseChar(char c)
        {
            if (!_tinyGps.encode(c) || !_tinyGps.location.isUpdated())
                return;

            _locationMutex.lock();

            _locationUpdatedEvent.Set();
            _location.age = xTaskGetTickCount();
            _location.latitude = _tinyGps.location.lat();
            _location.longitude = _tinyGps.location.lng();
            _location.accuracy = _tinyGps.hdop.hdop() * 20.0; //5 is a common multiplier for HDOP to meters, however in my testing 20 is more reasonable.

            if (!_tinyGps.date.isValid() || !_tinyGps.time.isValid())
            {
                _locationMutex.unlock();
                return;
            }

            tm timeInfo = {};
            timeInfo.tm_sec = _tinyGps.time.second();
            timeInfo.tm_min = _tinyGps.time.minute();
            timeInfo.tm_hour = _tinyGps.time.hour();
            timeInfo.tm_mday = _tinyGps.date.day();
            timeInfo.tm_mon = _tinyGps.date.month() - 1;
            timeInfo.tm_year = _tinyGps.date.year() - 1900;
            _location.timestamp = std::mktime(&timeInfo);

            _locationMutex.unlock();
        }

        void ReadGPS()
        {
            bool logVerbose = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE;
            #ifndef GPS_INTEGRATED
            while (GPS_UART.available())
            {
                char c = GPS_UART.read();
                #if true
                if (logVerbose)
                    WRITE(c);
                #endif
                ParseChar(c);
                if (logVerbose && _locationUpdatedEvent.IsSet())
                    LOGI(nameof(GPS), "Location: %f, %f", _location.latitude, _location.longitude);
            }
            #else
            #if true
            float lat2 = 0, lon2 = 0, speed2 = 0, alt2 = 0, accuracy2 = 0;
            int vsat2 = 0, usat2 = 0, year2 = 0, month2 = 0, day2 = 0, hour2 = 0, min2 = 0, sec2 = 0;
            uint8_t fixMode = 0;

            _modemMutex->lock();
            bool gotGps = _modem->getGPS(&fixMode, &lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2, &year2, &month2, &day2, &hour2, &min2, &sec2);
            _modemMutex->unlock();

            if (gotGps)
            {
                _locationMutex.lock();

                _locationUpdatedEvent.Set();
                _location.age = xTaskGetTickCount();
                _location.latitude = lat2;
                _location.longitude = lon2;
                _location.accuracy = accuracy2 * 20.0;

                tm timeInfo = {};
                timeInfo.tm_sec = sec2;
                timeInfo.tm_min = min2;
                timeInfo.tm_hour = hour2;
                timeInfo.tm_mday = day2;
                timeInfo.tm_mon = month2 - 1;
                timeInfo.tm_year = year2 - 1900;
                _location.timestamp = std::mktime(&timeInfo);

                _locationMutex.unlock();

                if (logVerbose)
                    LOGI(nameof(GPS), "Fix: %i Lat: %.6f, Lng: %.6f, Acc: %.6f", fixMode, _location.latitude, _location.longitude, _location.accuracy);
            }
            #else
            _modemMutex->lock();
            String gpsData = _modem->getGPSraw();
            _modemMutex->unlock();
            for (char c : gpsData)
            {
                ParseChar(c);
                if (logVerbose && _locationUpdatedEvent.IsSet())
                    LOGI(nameof(GPS), "Lat: %.6f, Lng: %.6f, Acc: %.6f", _location.latitude, _location.longitude, _location.accuracy);
            }
            #endif
            #endif
        }

        // #ifdef GPS_INTEGRATED
        // static void SecondaryTask(void* param)
        // {
        //     GPS* self = reinterpret_cast<GPS*>(param);

        //     while (!self->ServiceCancellationToken.IsCancellationRequested())
        //     {
        //         int year2 = 0, month2 = 0, day2 = 0, hour2 = 0, min2 = 0, sec2 = 0;

        //         self->_modemMutex->lock();
        //         bool gotTime = self->_modem->getGPSTime(&year2, &month2, &day2, &hour2, &min2, &sec2);
        //         self->_modemMutex->unlock();

        //         if (!gotTime)
        //         {
        //             vTaskDelay(pdMS_TO_TICKS(1000));
        //             continue;
        //         }

        //         tm timeInfo = {};
        //         timeInfo.tm_sec = sec2;
        //         timeInfo.tm_min = min2;
        //         timeInfo.tm_hour = hour2;
        //         timeInfo.tm_mday = day2;
        //         timeInfo.tm_mon = month2 - 1;
        //         timeInfo.tm_year = year2 - 1900;
        //         time_t timestamp = std::mktime(&timeInfo);

        //         //Store the GPS time in NVS for later use.
        //         SetConfig(GPS_TIME, (ulong)timestamp);
        //         Storage::Save();
        //         vTaskDelay(pdMS_TO_TICKS(5000));
        //     }

        //     vTaskDelete(NULL);
        // }
        // #endif

    protected:
        void RunServiceImpl() override
        {
            GSM* gsmService = GetService<GSM>();
            _modem = gsmService->GetModem();
            _modemMutex = gsmService->GetModemMutex();

            PowerOn();

            #ifdef BATTERY_ADC
            _batteryService = GetService<Battery>();
            _batteryService->OnBeforeSleep.Add([this](const Battery::ESleepType& sleepType)
            {
                switch (sleepType)
                {
                case Battery::ESleepType::Hibernate:
                case Battery::ESleepType::Deep:
                    //Module will be shutdown by the battery service.
                    // PowerOff();
                    break;
                case Battery::ESleepType::Light:
                    PowerOff();
                    break;
                default:
                    break;
                }
            });
            _batteryService->OnAfterSleep.Add([this](const Battery::ESleepType& sleepType)
            {
                switch (sleepType)
                {
                case Battery::ESleepType::Hibernate:
                case Battery::ESleepType::Deep:
                    //Won't be reached as deep sleep wakeup causes a system reset.
                    // PowerOn();
                    break;
                case Battery::ESleepType::Light:
                    PowerOn();
                    break;
                default:
                    break;
                }
            });
            #endif

            // #ifdef GPS_INTEGRATED
            // if (xTaskCreate(SecondaryTask, "gps_secondary", ServiceEntrypointStackDepth, this, ServiceEntrypointPriority, &_secondaryTaskHandle) != pdPASS)
            // {
            //     LOGE(nameof(GPS), "Failed to create GPS secondary task.");
            //     abort();
            // }
            // #endif
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                ReadGPS();
                vTaskDelay(_interval);
            }
            ServiceCancellationToken.WaitForCancellation();

            PowerOff();
            _modem = nullptr;
            _modemMutex = nullptr;
            #ifdef GPS_INTEGRATED
            // _secondaryTaskHandle = nullptr;
            #endif
        }

    public:
        GPS()
        {
            ServiceEntrypointStackDepth += 1024;
            #ifdef GPS_INTEGRATED
            AddDependencyType<GSM>();
            #endif
            SetupGPIO();
        }

        ~GPS()
        {
            PowerOff();
        }

        uint16_t GetPredictedTimeToFirstFix()
        {
            return _predictedTimeToFirstFix;
        }

        bool IsUpdated()
        {
            return _locationUpdatedEvent.IsSet();
        }

        void GetLocation(SLocation& outLocation)
        {
            _locationMutex.lock();
            outLocation = _location;
            _locationUpdatedEvent.Clear();
            _locationMutex.unlock();
        }

        void WaitForLocation(TickType_t timeout = portMAX_DELAY)
        {
            _locationUpdatedEvent.WaitOne(timeout);
        }

        TickType_t GetInterval()
        {
            return _interval;
        }
    };
};
