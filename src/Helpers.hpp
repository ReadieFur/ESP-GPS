#pragma once

#include <esp_sleep.h>

namespace ReadieFur::EspGps
{
    class Helpers
    {
    private:
        static uint64_t _deepSleepGpioPins;

    public:
        static void DeepSleepEnableGpioWakeup(gpio_num_t gpioPin)
        {
            _deepSleepGpioPins |= (1ULL << gpioPin);
        }

        static void DeepSleepDisableGpioWakeup(gpio_num_t gpioPin)
        {
            _deepSleepGpioPins &= ~(1ULL << gpioPin);
        }

        static esp_err_t DeepSleepEnableGpioWakeupMode(esp_deepsleep_gpio_wake_up_mode_t triggerType)
        {
            return esp_deep_sleep_enable_gpio_wakeup(_deepSleepGpioPins, triggerType);
        }
    };
};

uint64_t ReadieFur::EspGps::Helpers::_deepSleepGpioPins = 0;
