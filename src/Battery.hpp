#pragma once

#include <Arduino.h>
#include "Board.h"
#include <esp_sleep.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include "GSM.hpp"
#include <mutex>

class Battery
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

private:
    static std::mutex _mutex;
    static uint32_t _voltage, _solarVoltage;
    static EState _state;

    static uint SampleADC(int adcPin)
    {
        //Calculate the average power data.
        std::vector<uint32_t> data;
        for (int i = 0; i < 30; ++i)
        {
            //TODO: Detect that the battery is charging if the average of these samples keeps increasing steadily.
            uint32_t val = analogReadMilliVolts(adcPin);
            //SerialMon.printf("analogReadMilliVolts : %u mv \n", val * 2);
            data.push_back(val);
            delay(30);
        }
        std::sort(data.begin(), data.end());
        data.erase(data.begin());
        data.pop_back();

        int sum = std::accumulate(data.begin(), data.end(), 0);
        double average = static_cast<double>(sum) / data.size();
        average *= 2;

        return average;
    }

public:
    static void Init()
    {
        UpdateVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
        if (_state == (EState::Discharging | EState::Critical))
        {
            SerialMon.printf("Battery voltage is too low, %umv, entering sleep mode\n", _voltage);
            DeepSleep(GetConfig(int, BATTERY_CRIT_SLEEP));
        }
        SerialMon.printf("Battery voltage is %umV, solar voltage is %umV.\n", _voltage, _solarVoltage);
    }

    static void Loop()
    {
        UpdateVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after x period of time continue testing.
        if (_state == (EState::Discharging | EState::Critical))
        {
            SerialMon.printf("Battery voltage is low, %umv, entering sleep mode\n", _voltage);
            DeepSleep(GetConfig(int, BATTERY_CRIT_SLEEP));
        }
        // else if (_state < EState::Discharging_Low)
        // {
        //     SerialMon.println("Battery warning voltage level reached.");
        // }
    }

    static void GetStatus(uint32_t* outVoltage = nullptr, uint32_t* outSolarVoltage = nullptr, EState* outState = nullptr)
    {
        if (outVoltage != nullptr) *outVoltage = _voltage;
        if (outVoltage != nullptr) *outSolarVoltage = _solarVoltage;
        if (outState != nullptr) *outState = _state;
    }

    static void UpdateVoltage(uint32_t* outVoltage = nullptr, uint32_t* outSolarVoltage = nullptr, EState* outState = nullptr)
    {
        _mutex.lock();

        _voltage = SampleADC(BOARD_BAT_ADC);
        _solarVoltage = SampleADC(BOARD_SOLAR_ADC);

        if (_voltage <= BATTERY_CRIT_VOLTAGE)
            _state = EState::Critical;
        else if (_voltage <= BATTERY_LOW_VOLTAGE)
            _state = EState::Low;

        _state = (EState)(_state | (_solarVoltage >= CHG_VOLTAGE_MIN ? EState::Charging : EState::Discharging));

        if (outVoltage != nullptr) *outVoltage = _voltage;
        if (outVoltage != nullptr) *outSolarVoltage = _solarVoltage;
        if (outState != nullptr) *outState = _state;

        _mutex.unlock();
    }

    static uint GetSleepDuration()
    {
        switch (_state)
        {
        case EState::Discharging:
            return GetConfig(int, BATTERY_OK_INTERVAL);
        case EState::Discharging | EState::Low:
            return GetConfig(int, BATTERY_LOW_INTERVAL);
        case EState::Discharging | EState::Critical:
            return GetConfig(int, BATTERY_CRIT_SLEEP);
        default:
            //All charging states for now will use the single charging value.
            return GetConfig(int, BATTERY_CHRG_INTERVAL);
        }
    }

    static void LightSleep(uint32_t ms)
    {
        //TODO: Put the GPS module to sleep.
        GSM::Modem.sleepEnable(true);
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_light_sleep_start();
    }

    static void DeepSleep(uint32_t ms)
    {
        //TODO: Turn off the GPS module.
        GSM::Modem.poweroff();
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_deep_sleep_start();
    }
};

std::mutex Battery::_mutex;
uint32_t Battery::_voltage = 0;
uint32_t Battery::_solarVoltage = 0;
Battery::EState Battery::_state = Battery::EState::Discharging;
