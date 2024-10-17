#pragma once

#include <Arduino.h>
#include "Board.h"
#include <esp_sleep.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include "GSM.hpp"

class Battery
{
public:
    enum EState
    {
        Charging,
        Discharging,
        Discharging_Low,
        Discharging_Critical
    };

private:
    static uint32_t _voltage;
    static EState _state;

public:
    static void Init()
    {
        UpdateVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
        if (_state == EState::Discharging_Critical)
        {
            SerialMon.printf("Battery voltage is too low ,%umv, entering sleep mode\n", _voltage);
            DeepSleep(BATTERY_CRIT_SLEEP);
        }
        SerialMon.printf("Battery voltage is %umv\n", _voltage);
    }

    static void Loop()
    {
        UpdateVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after x period of time continue testing.
        if (_state == EState::Discharging_Critical)
        {
            SerialMon.printf("Battery voltage is low ,%umv, entering sleep mode\n", _voltage);
            DeepSleep(BATTERY_CRIT_SLEEP);
        }
        // else if (_state < EState::Discharging_Low)
        // {
        //     SerialMon.println("Battery warning voltage level reached.");
        // }
    }

    static void GetStatus(uint32_t* outVoltage = nullptr, EState* outState = nullptr)
    {
        if (outVoltage != nullptr) *outVoltage = _voltage;
        if (outState != nullptr) *outState = _state;
    }

    static void UpdateVoltage(uint32_t* outVoltage = nullptr, EState* outState = nullptr)
    {
        //Calculate the average power data.
        std::vector<uint32_t> data;
        for (int i = 0; i < 30; ++i)
        {
            //TODO: Detect that the battery is charging if the average of these samples keeps increasing steadily.
            uint32_t val = analogReadMilliVolts(BOARD_BAT_ADC);
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
        _voltage = average;

        if (_voltage >= BATTERY_CHG_VOLTAGE_HIGH || _voltage < BATTERY_CHG_VOLTAGE_LOW)
            _state = EState::Charging;
        else if (_voltage >= BATTERY_LOW_VOLTAGE)
            _state = Discharging;
        else if (_voltage >= BATTERY_CRIT_VOLTAGE)
            _state = Discharging_Low;
        else
            _state = Discharging_Critical;

        if (outVoltage != nullptr) *outVoltage = _voltage;
        if (outState != nullptr) *outState = _state;
    }

    static uint GetSleepDuration()
    {
        switch (_state)
        {
        case EState::Charging:
            return BATTERY_CHRG_INTERVAL;
        case EState::Discharging:
            return BATTERY_OK_INTERVAL;
        case EState::Discharging_Low:
            return BATTERY_LOW_INTERVAL;
        case EState::Discharging_Critical:
            return BATTERY_CRIT_SLEEP;
        default:
            //Shouldn't occur but assume low battery.
            return BATTERY_LOW_INTERVAL;
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

uint32_t Battery::_voltage = 0;
Battery::EState Battery::_state = Battery::EState::Discharging;
