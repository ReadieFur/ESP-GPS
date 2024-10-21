#pragma once

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "Board.h"
#include "Helpers.h"
#include <esp_log.h>

namespace ReadieFur::EspGps
{
    class Motion
    {
    public:
        static bool Configure()
        {
            TwoWire i2c(0);
            Adafruit_MPU6050 mpu;

            if (!i2c.begin(MPU_SDA, MPU_SCL))
            {
                ESP_LOGE(nameof(Motion), "I2C failed.");
                return false;
            }

            if (!mpu.begin((uint8_t)104U, &i2c))
            {
                ESP_LOGE(nameof(Motion), "MPU6050 not found.");
                return false;
            }
            ESP_LOGV(nameof(Motion), "MPU6050 found.");

            if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)MPU_INT))
            {
                ESP_LOGE(nameof(Motion), "MPU6050 interrupt pin invalid for wakeup.");
                return false;
            }
            ESP_LOGV(nameof(Motion), "MPU6050 interrupt pin valid.");

            mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
            // mpu.setMotionDetectionThreshold(GetConfig(int, MOTION_SENSITIVITY)); //0-255, ideal range seems to be between 7 and 12.
            // mpu.setMotionDetectionDuration(GetConfig(int, MOTION_DURATION)); //1 time unit is 10ms, so 2 seconds is 200 units.
            mpu.setInterruptPinLatch(false); //Testing with auto interrupt clear, should be ok.
            mpu.setInterruptPinPolarity(false);
            mpu.setMotionInterrupt(true);

            esp_sleep_enable_ext0_wakeup((gpio_num_t)MPU_INT, 1);

            ESP_LOGI(nameof(Motion), "Successfully configured MPU6050.");

            i2c.end();
            return true;
        }
    };
};
