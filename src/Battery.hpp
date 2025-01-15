#pragma once

// #ifdef BATTERY_ADC
#include <Arduino.h>
#include "Board.h"
#include <esp_sleep.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>
#include "Service/AService.hpp"
#include "Storage.hpp"
#include "Helpers.h"
#include "Logging.hpp"
#include <Event/Event.hpp>
#include <stdint.h>
#include <esp_adc_cal.h>
#include <WString.h>

#define __BATTERY_ADC_REF_VOLTAGE 3.3
#define __BATTERY_ADC_DIVIDER_RATIO ((BATTERY_DIV1 + BATTERY_DIV2) / BATTERY_DIV2)
#define __BATTERY_ADC_VREF 1100

namespace ReadieFur::EspGps
{
    class Battery : public Service::AService
    {
    public:
        enum EState
        {
            Charging = 1,
            Discharging = 2,
            // Full = 4,
            // Ok = 8,
            Low = 16,
            Critical = 32
        };

        enum ESleepType
        {
            Deep,
            Light,
            Task
        };

    private:
        std::mutex _mutex;
        esp_adc_cal_characteristics_t* adcChars;
        uint32_t _voltage = 0, _chargeVoltage = 0;
        EState _state = EState::Charging; //Assume we are plugged in by default.

        String MsToFormattedString(uint32_t ms)
        {
            uint32_t seconds = ms / 1000;
            uint32_t minutes = seconds / 60;
            uint32_t hours = minutes / 60;
            seconds %= 60;
            minutes %= 60;
            String result = "";
            if (hours > 0)
                result += String(hours) + "h ";
            if (minutes > 0)
                result += String(minutes) + "m ";
            if (seconds > 0)
                result += String(seconds) + "s";
            result.trim();
            if (result.isEmpty())
                return "0s";
            return result;
        }

        uint SampleADC(int adcPin)
        {
            //Calculate the average power data.
            std::vector<uint32_t> data;
            for (int i = 0; i < 30; ++i)
            {
                //TODO: Detect that the battery is charging if the average of these samples keeps increasing steadily.
                // uint32_t val = analogReadMilliVolts(adcPin);
                uint16_t rawAdc = analogRead(adcPin);
                uint32_t adcVoltageMv = esp_adc_cal_raw_to_voltage(rawAdc, adcChars);
                // double adcVoltage = (double)adcVoltageMv / 1000.0;
                double batteryVoltage = (double)adcVoltageMv * __BATTERY_ADC_DIVIDER_RATIO;
                // LOGV(nameof(Battery), "ADC: %u, %f", rawAdc, batteryVoltage);
                data.push_back(batteryVoltage);
                delay(30);
            }
            std::sort(data.begin(), data.end());
            data.erase(data.begin());
            data.pop_back();

            int sum = std::accumulate(data.begin(), data.end(), 0);
            double average = static_cast<double>(sum) / data.size();

            return average;
        }

        void UpdateVoltage()
        {
            _mutex.lock();

            _voltage = SampleADC(BATTERY_ADC);
            #ifdef CHARGE_ADC
            _chargeVoltage = SampleADC(CHARGE_ADC);
            #endif

            EState oldState = _state;

            if (_voltage <= BATTERY_CRIT_VOLTAGE)
                _state = EState::Critical;
            else if (_voltage <= BATTERY_LOW_VOLTAGE)
                _state = EState::Low;
            else if (_voltage < BATTERY_CHG_VOLTAGE)
                _state = EState::Discharging;
            else
                _state = EState::Charging;

            if (oldState != _state)
                OnStateChanged.Dispatch(_state);

            _mutex.unlock();
        }

    protected:
        void RunServiceImpl() override
        {
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                UpdateVoltage();
                #ifdef CHARGE_ADC
                LOGD(nameof(Battery), "Battery: %umV, Charge: %umV, State: %i", _voltage, _chargeVoltage, _state);
                #else
                LOGD(nameof(Battery), "Battery: %umV, State: %i", _voltage, _state);
                #endif

                //TODO: Check if the wake-up reason was due to motion, and if it was, don't force the device into sleep until one publish has been attempted.
                if (DoSystemManagement && _state & Critical)
                    Sleep();

                #ifdef TEST_BATTERY
                vTaskDelay(pdMS_TO_TICKS(1000));
                #else
                vTaskDelay(pdMS_TO_TICKS(1 * 1000));
                #endif
            }
        }

    public:
        bool DoSystemManagement = false;
        Event::Event<EState> OnStateChanged;
        Event::Event<ESleepType> OnBeforeSleep;
        Event::Event<ESleepType> OnAfterSleep;

        Battery()
        {
            ServiceEntrypointStackDepth += 2048;

            gpio_deep_sleep_hold_en();

            adcChars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
            esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, __BATTERY_ADC_VREF, adcChars);
            analogReadResolution(12);
            analogSetPinAttenuation(BATTERY_ADC, ADC_11db);
        }

        void Sleep()
        {
            uint64_t sleepTime = GetSleepDuration();

            switch (_state)
            {
                case EState::Critical:
                    LOGD(nameof(Battery), "Entering deep sleep for %s.", MsToFormattedString(sleepTime).c_str());
                    #ifndef TEST_BATTERY
                    OnBeforeSleep.Dispatch(ESleepType::Deep);
                    esp_deep_sleep(sleepTime * 1000);
                    OnAfterSleep.Dispatch(ESleepType::Deep); //Shouldn't ever be run because deep sleep will reset the program.
                    #endif
                    break;
                case EState::Low:
                case EState::Discharging:
                    LOGD(nameof(Battery), "Entering light sleep for %s.", MsToFormattedString(sleepTime).c_str());
                    #ifndef TEST_BATTERY
                    OnBeforeSleep.Dispatch(ESleepType::Light);
                    esp_sleep_enable_timer_wakeup(sleepTime * 1000);
                    esp_light_sleep_start();
                    OnAfterSleep.Dispatch(ESleepType::Light);
                    #endif
                    break;
                case EState::Charging:
                default:
                    LOGD(nameof(Battery), "Entering task sleep for %s.", MsToFormattedString(sleepTime).c_str());
                    #ifndef TEST_BATTERY
                    OnBeforeSleep.Dispatch(ESleepType::Task);
                    vTaskDelay(pdMS_TO_TICKS(sleepTime));
                    OnAfterSleep.Dispatch(ESleepType::Task);
                    #endif
                    break;
            }
        }

        uint GetSleepDuration()
        {
            switch (_state)
            {
            case /*EState::Discharging |*/ EState::Critical:
                return GetConfig(int, BATTERY_CRIT_SLEEP);
            case /*EState::Discharging |*/ EState::Low:
                return GetConfig(int, BATTERY_LOW_INTERVAL);
            case EState::Discharging:
                return GetConfig(int, BATTERY_OK_INTERVAL);
            case EState::Charging:
            default:
                return GetConfig(int, BATTERY_CHRG_INTERVAL);
            }
        }

        void GetStatus(double* batteryVoltage, double* chargeVoltage, EState* state)
        {
            if (batteryVoltage != nullptr) *batteryVoltage = _voltage;
            if (chargeVoltage != nullptr) *chargeVoltage = _chargeVoltage;
            if (state != nullptr) *state = _state;
        }
    };
};
// #endif
