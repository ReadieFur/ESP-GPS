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
private:
    static uint32_t _voltageInterval;

    static void DeepSleep(uint32_t ms)
    {
        //TODO: Turn off the GPS module.
        GSM::Modem.poweroff();
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_deep_sleep_start();
    }

public:
    enum EState
    {
        Charging,
        Discharging,
        Discharging_Low,
        Discharging_Critical
    };

    static EState State;

    static void Init()
    {
        //Disable the built in WiFi modem 

        uint32_t batteryVoltageMv = GetVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
        if (batteryVoltageMv < BATTERY_CRIT_VOLTAGE)
        {
            SerialMon.printf("Battery voltage is too low ,%u mv, entering sleep mode\n", batteryVoltageMv);
            DeepSleep(BATTERY_CRIT_SLEEP);
        }
        SerialMon.printf("Battery voltage is %u mv\n", batteryVoltageMv);
    }

    static void Loop()
    {
        if (millis() < _voltageInterval)
            return;
        
        //Check the battery voltage every x milliseconds.
        _voltageInterval = millis() + 5000; //TODO: Change this to the update interval.

        uint32_t batteryVoltageMv = GetVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after x period of time continue testing.
        if (batteryVoltageMv < BATTERY_CRIT_VOLTAGE)
        {
            SerialMon.printf("Battery voltage is low ,%u mv, entering sleep mode\n", batteryVoltageMv);
            DeepSleep(BATTERY_CRIT_SLEEP);
        }
        else if (batteryVoltageMv < BATTERY_LOW_VOLTAGE)
        {
            SerialMon.println("Battery warning voltage level reached.");
        }
    }

    static uint32_t GetVoltage()
    {
        //Calculate the average power data.
        std::vector<uint32_t> data;
        for (int i = 0; i < 30; ++i)
        {
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

        if (average > 4100)
            State = EState::Charging;
        else if (average > BATTERY_LOW_VOLTAGE)
            State = Discharging;
        else if (average > BATTERY_CRIT_VOLTAGE)
            State = Discharging_Low;
        else
            State = Discharging_Critical;

        return average;
    }

    static uint GetSleepDuration()
    {
        switch (State)
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
};

uint32_t Battery::_voltageInterval = 0;
Battery::EState Battery::State = Battery::EState::Discharging;
