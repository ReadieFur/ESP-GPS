#pragma once

//https://registry.platformio.org/libraries/adafruit/Adafruit%20MPU6050/examples/motion_detection/motion_detection.ino
//https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
//https://microcontrollerslab.com/esp32-external-interrupts-wake-up-deep-sleep/

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <esp_sleep.h>
#include "Board.h"

class Motion
{
private:
    static TwoWire i2c;
    static Adafruit_MPU6050 mpu;
    static bool setupFailed;

public:
    static bool Init()
    {
        if (i2c.begin(MPU_SDA, MPU_SCL))
        {
            SerialMon.println("I2C failed.");
            setupFailed = true;
            return false;
        }

        if (!mpu.begin((uint8_t)104U, &i2c))
        {
            SerialMon.println("MPU6050 not found.");
            setupFailed = true;
            return false;
        }
        SerialMon.println("MPU6050 found.");

        //Clear any interrupts.
        mpu.getEvent(nullptr, nullptr, nullptr);

        mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
        mpu.setMotionDetectionThreshold(1);
        mpu.setMotionDetectionDuration(20);
        mpu.setInterruptPinLatch(true);
        mpu.setInterruptPinPolarity(false);
        mpu.setMotionInterrupt(true);

        esp_sleep_enable_ext0_wakeup((gpio_num_t)MPU_INT, 1);

        return true;
    }

    static void Loop()
    {
        if (!setupFailed)
            mpu.getEvent(nullptr, nullptr, nullptr);
    }
};

TwoWire Motion::i2c = TwoWire(0);
Adafruit_MPU6050 Motion::mpu;
bool Motion::setupFailed = false;
