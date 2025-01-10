#pragma once

// #ifdef MPU_INT
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "Board.h"
#include "Helpers.h"
#include "Logging.hpp"
#include "Storage.hpp"

namespace ReadieFur::EspGps
{
    class Motion
    {
    public:
        static bool Configure()
        {
            esp_err_t err;
            TwoWire i2c(0);
            Adafruit_MPU6050 mpu;

            if (!i2c.begin(MPU_SDA, MPU_SCL))
            {
                LOGE(nameof(Motion), "I2C failed.");
                return false;
            }

            if (!mpu.begin((uint8_t)104U, &i2c))
            {
                LOGE(nameof(Motion), "MPU6050 not found.");
                return false;
            }
            LOGV(nameof(Motion), "MPU6050 found.");

            if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)MPU_INT))
            {
                LOGE(nameof(Motion), "MPU6050 interrupt pin invalid for wakeup.");
                return false;
            }
            LOGV(nameof(Motion), "MPU6050 interrupt pin valid.");

            mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
            mpu.setMotionDetectionThreshold(GetConfig(int, MOTION_SENSITIVITY)); //0-255, ideal range seems to be between 7 and 12.
            mpu.setMotionDetectionDuration(GetConfig(int, MOTION_DURATION)); //1 time unit is 10ms, so 2 seconds is 200 units.
            mpu.setInterruptPinLatch(false); //Testing with auto interrupt clear, should be ok.
            mpu.setInterruptPinPolarity(false);
            mpu.setMotionInterrupt(true);

            //Light sleep wakeup.
            #if SOC_PM_SUPPORT_EXT_WAKEUP
            err = esp_sleep_enable_ext0_wakeup((gpio_num_t)MPU_INT, 1);
            #else
            err = gpio_wakeup_enable((gpio_num_t)MPU_INT, GPIO_INTR_HIGH_LEVEL);
            #endif
            if (err != ESP_OK)
            {
                LOGE(nameof(Motion), "Failed to enable wakeup on MPU6050 interrupt.");
                return false;
            }

            //Deep sleep wakeup.
            //TODO: Set these globally so multiple "modules" can configure their own wakeup sources without unconfigring others.
            err = esp_deep_sleep_enable_gpio_wakeup((1ULL << MPU_INT), ESP_GPIO_WAKEUP_GPIO_HIGH);
            if (err != ESP_OK)
            {
                LOGE(nameof(Motion), "Failed to enable GPIO wakeup on MPU6050 interrupt.");
                return false;
            }

            LOGI(nameof(Motion), "Successfully configured MPU6050.");

            i2c.end();
            return true;
        }
    };
};
// #endif
