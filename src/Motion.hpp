#pragma once

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "Board.h"
#include "Helpers.h"
#include "Logging.hpp"
#include "Storage.hpp"
#include <Service/AService.hpp>
#ifdef BATTERY_ADC
#include "Battery.hpp"
#endif

namespace ReadieFur::EspGps
{
    class Motion : public Service::AService
    {
    private:
        TwoWire _i2c = TwoWire(0);
        Adafruit_MPU6050 _mpu;
        #ifdef BATTERY_ADC
        Battery* _batteryService;
        #endif
    
    protected:
        void RunServiceImpl() override
        {
            esp_err_t err;

            if (!_i2c.begin(MPU_SDA, MPU_SCL))
            {
                LOGE(nameof(Motion), "I2C failed.");
                abort();
            }

            if (!_mpu.begin((uint8_t)104U, &_i2c))
            {
                LOGE(nameof(Motion), "MPU6050 not found.");
                abort();
            }
            LOGV(nameof(Motion), "MPU6050 found.");

            if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)MPU_INT))
            {
                LOGE(nameof(Motion), "MPU6050 interrupt pin invalid for wakeup.");
                abort();
            }

            _mpu.enableSleep(false);
            _mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
            _mpu.setMotionDetectionThreshold(GetConfig(int, MOTION_SENSITIVITY)); //0-255, ideal range seems to be between 7 and 12.
            _mpu.setMotionDetectionDuration(GetConfig(int, MOTION_DURATION)); //1 time unit is 10ms, so 2 seconds is 200 units.
            _mpu.setInterruptPinLatch(false); //Testing with auto interrupt clear, should be ok.
            _mpu.setInterruptPinPolarity(false);
            _mpu.setMotionInterrupt(true);

            //Light sleep wakeup.
            #if SOC_PM_SUPPORT_EXT_WAKEUP
            err = esp_sleep_enable_ext0_wakeup((gpio_num_t)MPU_INT, 1);
            #else
            err = gpio_wakeup_enable((gpio_num_t)MPU_INT, GPIO_INTR_HIGH_LEVEL);
            #endif
            if (err != ESP_OK)
            {
                LOGE(nameof(Motion), "Failed to enable wakeup on MPU6050 interrupt.");
                abort();
            }

            //Deep sleep wakeup.
            //TODO: Set these globally so multiple "modules" can configure their own wakeup sources without unconfigring others.
            err = esp_deep_sleep_enable_gpio_wakeup((1ULL << MPU_INT), ESP_GPIO_WAKEUP_GPIO_HIGH);
            if (err != ESP_OK)
            {
                LOGE(nameof(Motion), "Failed to enable GPIO wakeup on MPU6050 interrupt.");
                abort();
            }

            #ifdef BATTERY_ADC
            _batteryService = GetService<Battery>();
            _batteryService->OnBeforeSleep.Add([this](const Battery::ESleepType& sleepType)
            {
                switch (sleepType)
                {
                case Battery::ESleepType::Deep:
                    _mpu.enableSleep(true);
                    break;
                default:
                    break;
                }
            });
            _batteryService->OnAfterSleep.Add([this](const Battery::ESleepType& sleepType)
            {
                switch (sleepType)
                {
                case Battery::ESleepType::Deep:
                    _mpu.enableSleep(false);
                    break;
                default:
                    break;
                }
            });
            #endif

            LOGI(nameof(Motion), "Successfully configured MPU6050.");

            ServiceCancellationToken.WaitForCancellation();

            _i2c.end();
        }
    
    public:
        Motion()
        {
            ServiceEntrypointStackDepth += 1024;
            #ifdef BATTERY_ADC
            AddDependencyType<Battery>();
            #endif
        }
    };
};
