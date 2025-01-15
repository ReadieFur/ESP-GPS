#pragma once

// #ifdef BATTERY_ADC
#include <freertos/FreeRTOS.h>
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
#include <freertos/task.h>
#include <Event/ManualResetEvent.hpp>
#include <Service/ServiceManager.hpp>
#ifdef DEBUG
#include <Diagnostic/DiagnosticsService.hpp>
#endif
// #define SHUTDOWN_SERIAL_MONITOR
#ifdef SHUTDOWN_SERIAL_MONITOR
#include "SerialMonitor.hpp"
#endif
#include <esp_timer.h>

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
            Hibernate,
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
        Event::ManualResetEvent _sleepInterrupt;

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

        String SleepTypeToString(ESleepType sleepType)
        {
            switch (sleepType)
            {
            case ESleepType::Hibernate:
                return "Hibernate";
            case ESleepType::Deep:
                return "Deep";
            case ESleepType::Light:
                return "Light";
            case ESleepType::Task:
                return "Task";
            default:
                return "Unknown";
            }
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

        //Run sleep tasks on this thread rather than the one the calls the sleep function.
        void SleepInternal()
        {
            uint64_t sleepTime = GetSleepDuration();

            //TODO: Make this configurable as to which mode should be used for a given battery state.
            ESleepType sleepType;
            if (_state & EState::Charging)
                sleepType = ESleepType::Light;
            else if (_state & EState::Critical) //TODO: Debate wether this state should be used even when charging if the battery is critically low.
                sleepType = ESleepType::Hibernate;
            else if (_state & EState::Low)
                sleepType = ESleepType::Deep;
            else if (_state & EState::Ok)
                sleepType = ESleepType::Deep;
            else
            {
                LOGE(nameof(Battery), "Failed to sleep, unknown state: %s", StateToString(_state).c_str());
                return;
            }

            LOGD(nameof(Battery), "Entering %s sleep for %s...", SleepTypeToString(sleepType).c_str(), MsToFormattedString(sleepTime).c_str());
            vTaskDelay(pdMS_TO_TICKS(100)); //Give the system a chance to log the message before sleeping.

            OnBeforeSleep.Dispatch(sleepType);

            #if !defined(TEST_BATTERY) || true
            switch (sleepType)
            {
            case ESleepType::Hibernate:
            {
                //https://m1cr0lab-esp32.github.io/sleep-modes/hibernation-mode/
                esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
                esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
                esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
                esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
                //Fall through to the next block (deep sleep).
            }
            case ESleepType::Deep:
            {
                static const std::vector<std::type_index> exemptServices =
                {
                    #ifdef DEBUG
                    std::type_index(typeid(ReadieFur::Diagnostic::DiagnosticsService)),
                    #endif
                    #ifdef SHUTDOWN_SERIAL_MONITOR
                    std::type_index(typeid(ReadieFur::EspGps::SerialMonitor)),
                    #endif
                    std::type_index(typeid(Battery))
                };
                std::vector<std::type_index> services = ReadieFur::Service::ServiceManager::GetServices();
                //Iterate in reverse as the last item is the first to shutdown.
                for (auto it = services.rbegin(); it != services.rend(); ++it)
                {
                    if (std::find(exemptServices.begin(), exemptServices.end(), *it) != exemptServices.end())
                        continue;

                    ReadieFur::Service::EServiceResult res = ReadieFur::Service::ServiceManager::StopService(*it); //This waits for the service to end.
                    if (res != ReadieFur::Service::EServiceResult::Ok)
                        LOGW(nameof(Battery), "Failed to stop service: %s, %i", it->name(), res);
                    else
                        LOGV(nameof(Battery), "Stopped service: %s", it->name());
                }
                esp_deep_sleep(sleepTime * 1000);
                break;
            }
            case ESleepType::Light:
            {
                esp_sleep_enable_timer_wakeup(sleepTime * 1000);
                esp_light_sleep_start();
                break;
            }
            case ESleepType::Task:
            {
                #if false
                vTaskSuspendAll(); //Pause all other code execution (only the calling context remains active because task switching is disabled, interrupts are still active).
                vTaskDelay(pdMS_TO_TICKS(sleepTime)); //Cannot be called while the task scheduler is suspended.
                xTaskResumeAll();
                #elif true
                vTaskSuspendAll();
                ets_delay_us(sleepTime * 1000); //Use the esp32 delay function instead, this halts the entire CPU I believe, not just the FreeRTOS task scheduler. This is a busy loop internally which isn't ideal for power consumption.
                //Other alternative is to use light sleep again.
                xTaskResumeAll();
                #else
                esp_sleep_enable_timer_wakeup(sleepTime * 1000);
                esp_light_sleep_start();
                #endif
                break;
            }
            default:
                break;
            }
            #endif

            _sleepInterrupt.Clear();

            OnAfterSleep.Dispatch(sleepType);
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
                if ((DoSystemManagement && _state & Critical) || _sleepInterrupt.IsSet())
                    SleepInternal();

                #ifdef TEST_BATTERY
                vTaskDelay(_sleepInterrupt.WaitOne(pdMS_TO_TICKS(1000))); //Wait for a sleep signal before the next iteration or for one second to pass.
                #else
                vTaskDelay(_sleepInterrupt.WaitOne(pdMS_TO_TICKS(1 * 1000)));
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
        }

        ~Battery()
        {
            free(_adcChars);
        }

        uint GetSleepDuration()
        {
            #if !defined(TEST_BATTERY) || false
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
            #else
            return 5000;
            #endif
        }

        void Sleep()
        {
            _sleepInterrupt.Set();
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
