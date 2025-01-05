#pragma once

#include "Board.h"
#include "Config.h"
#include "Service/AService.hpp"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "Logging.hpp"
#include "Helpers.h"
#include "SLocation.h"

#if !defined(GPS_RX) || !defined(GPS_TX)
#define GPS_INTEGRATED
#include "GSM.hpp"
#endif

namespace ReadieFur::EspGps
{
    class GPS : public Service::AService
    {
    private:
        TinyGPSPlus _tinyGps;
        bool _locationUpdated = false;
        SLocation _location;
        TickType_t _interval = pdMS_TO_TICKS(1000);
        #ifdef GPS_INTEGRATED
        TinyGsm* _modem;
        std::mutex* _mutex;
        #endif

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
            #else
            _mutex->lock();
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
                //TODO: Dynamically pick between using cold, warm and hot start.
            }

            _modem->setGPSBaud(115200);
            // _modem->setGPSMode(7);

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

            _mutex->unlock();
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
            _mutex->lock();
            _modem->disableGPS();
            _mutex->unlock();
            #endif
        }

        void ParseChar(char c)
        {
            if (!_tinyGps.encode(c) || !_tinyGps.location.isUpdated())
                return;

            _locationUpdated = true;
            _location.age = millis();
            _location.latitude = _tinyGps.location.lat();
            _location.longitude = _tinyGps.location.lng();
            _location.accuracy = _tinyGps.hdop.hdop();

            if (!_tinyGps.date.isValid() || !_tinyGps.time.isValid())
                return;

            tm timeInfo = {};
            timeInfo.tm_sec = _tinyGps.time.second();
            timeInfo.tm_min = _tinyGps.time.minute();
            timeInfo.tm_hour = _tinyGps.time.hour();
            timeInfo.tm_mday = _tinyGps.date.day();
            timeInfo.tm_mon = _tinyGps.date.month() - 1;
            timeInfo.tm_year = _tinyGps.date.year() - 1900;
            _location.timestamp = std::mktime(&timeInfo);
        }

        void ReadGPS()
        {
            bool logVerbose = esp_log_level_get(nameof(GPS)) >= esp_log_level_t::ESP_LOG_VERBOSE;
            #ifndef GPS_INTEGRATED
            while (Serial1.available())
            {
                char c = Serial1.read();
                #if true
                if (logVerbose)
                    WRITE(c);
                #endif
                ParseChar(c);
                if (logVerbose && _locationUpdated)
                    LOGI(nameof(GPS), "Location: %f, %f", _location.latitude, _location.longitude);
            }
            #else
            _mutex->lock();
            #if true
            float lat2 = 0, lon2 = 0, speed2 = 0, alt2 = 0, accuracy2 = 0;
            int vsat2 = 0, usat2 = 0, year2 = 0, month2 = 0, day2 = 0, hour2 = 0, min2 = 0, sec2 = 0;
            uint8_t fixMode = 0;
            if (_modem->getGPS(&fixMode, &lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2, &year2, &month2, &day2, &hour2, &min2, &sec2))
            {
                _locationUpdated = true;
                _location.age = millis();
                _location.latitude = lat2;
                _location.longitude = lon2;
                _location.accuracy = accuracy2;

                tm timeInfo = {};
                timeInfo.tm_sec = sec2;
                timeInfo.tm_min = min2;
                timeInfo.tm_hour = hour2;
                timeInfo.tm_mday = day2;
                timeInfo.tm_mon = month2 - 1;
                timeInfo.tm_year = year2 - 1900;
                _location.timestamp = std::mktime(&timeInfo);

                if (logVerbose)
                    LOGI(nameof(GPS), "Location: %f, %f", _location.latitude, _location.longitude);
            }
            #else
            String gpsData = _modem->getGPSraw();
            for (char c : gpsData)
            {
                ParseChar(c);
                if (logVerbose && _locationUpdated)
                    LOGI(nameof(GPS), "Location: %f, %f", _location.latitude, _location.longitude);
            }
            #endif
            _mutex->unlock();
            #endif
        }

    protected:
        void RunServiceImpl() override
        {
            GSM* gsmService = GetService<GSM>();
            _modem = gsmService->GetModem();
            _mutex = gsmService->GetModemMutex();
            PowerOn();

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                ReadGPS();
                vTaskDelay(_interval);
            }

            PowerOff();
            _modem = nullptr;
            _mutex = nullptr;
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

        bool IsUpdated()
        {
            return _locationUpdated;
        }

        SLocation GetLocation()
        {
            _locationUpdated = false;
            return _location;
        }
    };
};
