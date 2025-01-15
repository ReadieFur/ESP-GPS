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
#include <stdint.h>
#include "Helpers.hpp"

#define __BATTERY_ADC_REF_VOLTAGE 3.3
#define __BATTERY_ADC_VREF 1100
#define __BATTERY_ADC_DIVIDER_RATIO ((BATTERY_DIV1 + BATTERY_DIV2) / BATTERY_DIV2)
#ifdef CHARGE_ADC
#define __CHARGE_ADC_DIVIDER_RATIO ((CHARGE_DIV1 + CHARGE_DIV2) / CHARGE_DIV2)
#endif

namespace ReadieFur::EspGps
{
    class Battery : public Service::AService
    {
    public:
        enum EState
        {
            Unknown = 1 << 0,
            Charging = 1 << 1,
            Discharging = 1 << 2,
            // Full = 1 << 3,
            Ok = 1 << 4,
            Low = 1 << 5,
            Critical = 1 << 6
        };

        enum ESleepType
        {
            Deep,
            Light,
            Task
        };

    private:
        std::mutex _mutex;
        esp_adc_cal_characteristics_t* _adcChars;
        uint32_t _batteryVoltage = 0;
        #ifdef CHARGE_ADC
        uint32_t _chargeVoltage = 0;
        #endif
        EState _state = EState::Unknown;

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

        String StateToString(EState state)
        {
            String result = "";
            if (state & Unknown)
                result += "Unknown ";
            if (state & Charging)
                result += "Charging ";
            if (state & Discharging)
                result += "Discharging ";
            // if (state & Full)
            //     result += "Full ";
            if (state & Ok)
                result += "Ok ";
            if (state & Low)
                result += "Low ";
            if (state & Critical)
                result += "Critical ";
            result.trim();
            return result;
        }

        uint SampleADC(int adcPin, float dividerRatio = 1.0)
        {
            //Calculate the average power data.
            std::vector<uint32_t> data;
            for (int i = 0; i < 30; ++i)
            {
                //TODO: Detect that the battery is charging if the average of these samples keeps increasing steadily.
                // uint32_t val = analogReadMilliVolts(adcPin);
                uint16_t rawAdc = analogRead(adcPin);
                uint32_t adcVoltageMv = esp_adc_cal_raw_to_voltage(rawAdc, _adcChars);
                // double adcVoltage = (double)adcVoltageMv / 1000.0;
                double voltage = (double)adcVoltageMv * dividerRatio;
                #if defined(TEST_BATTERY) && false
                LOGV(nameof(Battery), "ADC: %u, %f", rawAdc, voltage);
                #endif
                data.push_back(voltage);
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

            _batteryVoltage = SampleADC(BATTERY_ADC, __BATTERY_ADC_DIVIDER_RATIO);
            #ifdef CHARGE_ADC
            _chargeVoltage = SampleADC(CHARGE_ADC, __CHARGE_ADC_DIVIDER_RATIO);
            #endif

            EState newState;

            if (_batteryVoltage <= BATTERY_CRIT_VOLTAGE)
                newState = EState::Critical;
            else if (_batteryVoltage <= BATTERY_LOW_VOLTAGE)
                newState = EState::Low;
            else if (_batteryVoltage >= BATTERY_OK_VOLTAGE)
                newState = EState::Ok;

            #ifdef CHARGE_ADC
            //OR in the charging state.
            if (_chargeVoltage > 1000) //TODO: Make this a configurable value.
                newState = static_cast<EState>(newState | EState::Charging);
            else
                newState = static_cast<EState>(newState | EState::Discharging);
            #else
            _state |= EState::Charging;
            #endif

            if (newState != _state)
            {
                _state = newState;
                _mutex.unlock();
                OnStateChanged.Dispatch(newState);
            }
            else
            {
                _mutex.unlock();
            }
        }

    protected:
        void RunServiceImpl() override
        {
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                UpdateVoltage();
                #ifdef CHARGE_ADC
                LOGD(nameof(Battery), "Battery: %umV, Charge: %umV, State: %s", _batteryVoltage, _chargeVoltage, StateToString(_state).c_str());
                #else
                LOGD(nameof(Battery), "Battery: %umV, State: %s", _batteryVoltage, StateToString(_state).c_str());
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

            // gpio_deep_sleep_hold_en();
            gpio_deep_sleep_hold_dis();
            esp_sleep_config_gpio_isolate();

            esp_err_t err;

            _adcChars = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
            esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, __BATTERY_ADC_VREF, _adcChars);
            analogReadResolution(12);

            pinMode(BATTERY_ADC, INPUT_PULLDOWN);
            if (err != ESP_OK)
            {
                LOGE(nameof(Battery), "Failed to configure battery ADC pin: %s", esp_err_to_name(err));
                abort();
            }
            analogSetPinAttenuation(BATTERY_ADC, ADC_11db);

            #ifdef CHARGE_ADC
            if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)CHARGE_ADC))
            {
                LOGE(nameof(Battery), "Invalid wake-up GPIO: %i", CHARGE_ADC);
                abort();
            }
            pinMode(CHARGE_ADC, INPUT);
            analogSetPinAttenuation(CHARGE_ADC, ADC_11db);
            //TODO: Check why this isn't waking up the device from sleep.
            err = gpio_wakeup_enable((gpio_num_t)MPU_INT, GPIO_INTR_HIGH_LEVEL);
            err = esp_sleep_enable_gpio_wakeup();
            Helpers::DeepSleepEnableGpioWakeup((gpio_num_t)CHARGE_ADC);
            Helpers::DeepSleepEnableGpioWakeupMode(ESP_GPIO_WAKEUP_GPIO_HIGH); //Preferred GPIO_INTR_POSEDGE but not valid in this case.
            if (err != ESP_OK)
            {
                LOGE(nameof(Battery), "Failed to enable charge ADC wake-up: %s", esp_err_to_name(err));
                abort();
            }
            #endif

            #ifdef TEST_BATTERY
            DoSystemManagement = true;
            #endif
        }

        ~Battery()
        {
            free(_adcChars);
        }

        uint GetSleepDuration()
        {
            if (_state & EState::Charging)
                return GetConfig(int, BATTERY_CHRG_INTERVAL);
            else if (_state & EState::Critical)
                return GetConfig(int, BATTERY_CRIT_SLEEP);
            else if (_state & EState::Low)
                return GetConfig(int, BATTERY_LOW_INTERVAL);
            else if (_state & EState::Ok)
                return GetConfig(int, BATTERY_OK_INTERVAL);
            else //Shouldn't be reached.
                return GetConfig(int, BATTERY_OK_INTERVAL);
        }

        void Sleep()
        {
            uint64_t sleepTime = GetSleepDuration();

            if (_state & EState::Charging)
            {
                LOGD(nameof(Battery), "Entering task sleep for %s.", MsToFormattedString(sleepTime).c_str());
                #ifndef TEST_BATTERY
                OnBeforeSleep.Dispatch(ESleepType::Task);
                vTaskDelay(pdMS_TO_TICKS(sleepTime));
                OnAfterSleep.Dispatch(ESleepType::Task);
                #endif
            }
            else if (_state & EState::Critical) //TODO: Debate wether this state should be used even when charging if the battery is critically low.
            {
                LOGD(nameof(Battery), "Entering deep sleep for %s.", MsToFormattedString(sleepTime).c_str());
                #ifndef TEST_BATTERY
                OnBeforeSleep.Dispatch(ESleepType::Deep);
                esp_deep_sleep(sleepTime * 1000);
                OnAfterSleep.Dispatch(ESleepType::Deep); //Shouldn't ever be run because deep sleep will reset the program.
                #endif
            }
            else if (_state & EState::Low || _state & EState::Ok)
            {
                LOGD(nameof(Battery), "Entering light sleep for %s.", MsToFormattedString(sleepTime).c_str());
                #ifndef TEST_BATTERY
                OnBeforeSleep.Dispatch(ESleepType::Light);
                esp_sleep_enable_timer_wakeup(sleepTime * 1000);
                esp_light_sleep_start();
                OnAfterSleep.Dispatch(ESleepType::Light);
                #endif
            }
            else
            {
                LOGE(nameof(Battery), "Failed to sleep, unknown state: %s", StateToString(_state).c_str());
            }
        }

        void GetStatus(double* batteryVoltage, double* chargeVoltage, EState* state)
        {
            if (batteryVoltage != nullptr) *batteryVoltage = _batteryVoltage;
            #ifdef CHARGE_ADC
            if (chargeVoltage != nullptr) *chargeVoltage = _chargeVoltage;
            #else
            *chargeVoltage = 0;
            #endif
            if (state != nullptr) *state = _state;
        }
    };
};
// #endif
