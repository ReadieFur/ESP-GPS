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
    static uint32_t voltage_interval;

    static void light_sleep(uint32_t ms)
    {
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_light_sleep_start();
    }

    static void deep_sleep(uint32_t ms)
    {
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_deep_sleep_start();
    }

    static uint32_t getBatteryVoltage()
    {
        // Calculate the average power data
        std::vector<uint32_t> data;
        for (int i = 0; i < 30; ++i)
        {
            uint32_t val = analogReadMilliVolts(BOARD_BAT_ADC);
            // Serial.printf("analogReadMilliVolts : %u mv \n", val * 2);
            data.push_back(val);
            delay(30);
        }
        std::sort(data.begin(), data.end());
        data.erase(data.begin());
        data.pop_back();
        int sum = std::accumulate(data.begin(), data.end(), 0);
        double average = static_cast<double>(sum) / data.size();
        return  average * 2;
    }

public:
    static void Init()
    {
        uint32_t battery_voltage_mv = getBatteryVoltage();
        // If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.
        if (battery_voltage_mv < BATTERY_LOW_LEVEL)
        {
            Serial.printf("Battery voltage is too low ,%u mv, entering sleep mode\n", battery_voltage_mv);
            deep_sleep(BATTERY_LOW_SLEEP_TIME);
        }
        Serial.printf("Battery voltage is %u mv\n", battery_voltage_mv);
    }

    static void Loop()
    {
        if (millis() > voltage_interval)
        {
            // Check the battery voltage every 30 seconds
            voltage_interval = millis() + 30000;

            uint32_t battery_voltage_mv = getBatteryVoltage();

            // If the battery level is lower than 3.6V, the system will continue to sleep and wake up after one hour to continue testing.

            if (battery_voltage_mv < BATTERY_LOW_LEVEL)
            {
                Serial.printf("Battery voltage is too low ,%u mv, entering sleep mode\n", battery_voltage_mv);

                // Turn off the modem
                GSM::modem.poweroff();

                // Sleep esp32
                deep_sleep(BATTERY_LOW_SLEEP_TIME); //60 minute

            }
            else if (battery_voltage_mv < BATTERY_WARN_LEVEL)
            {

                Serial.println("Battery voltage reaches the warning voltage");

            }

        }
    }
};

uint32_t Battery::voltage_interval = 0;
