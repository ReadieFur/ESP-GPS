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

    static void LightSleep(uint32_t ms)
    {
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_light_sleep_start();
    }

    static void DeepSleep(uint32_t ms)
    {
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_deep_sleep_start();
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
        return average * 2;
    }

public:
    static void Init()
    {
        uint32_t batteryVoltageMv = GetVoltage();
        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
        if (batteryVoltageMv < BATTERY_LOW_LEVEL)
        {
            SerialMon.printf("Battery voltage is too low ,%u mv, entering sleep mode\n", batteryVoltageMv);
            DeepSleep(BATTERY_LOW_SLEEP_TIME);
        }
        SerialMon.printf("Battery voltage is %u mv\n", batteryVoltageMv);
    }

    static void Loop()
    {
        if (millis() < _voltageInterval)
            return;
        
        //Check the battery voltage every x milliseconds.
        _voltageInterval = millis() + 30000; //TODO: Change this to the update interval.

        uint32_t batteryVoltageMv = GetVoltage();

        //If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
        if (batteryVoltageMv < BATTERY_LOW_LEVEL)
        {
            SerialMon.printf("Battery voltage is low ,%u mv, entering sleep mode\n", batteryVoltageMv);

            //Turn off the modem and enter deep sleep.
            //TODO: Put the GPS to sleep.
            GSM::Modem.poweroff();
            DeepSleep(BATTERY_LOW_SLEEP_TIME);
        }
        else if (batteryVoltageMv < BATTERY_WARN_LEVEL)
        {
            SerialMon.println("Battery warning voltage level reached.");
        }
    }
};

uint32_t Battery::_voltageInterval = 0;
