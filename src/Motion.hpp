#pragma once

#define MOTION_MODULE

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
        if (!i2c.begin(MPU_SDA, MPU_SCL))
        {
            SerialMon.println("I2C failed.");
            return false;
        }

        if (!mpu.begin((uint8_t)104U, &i2c))
        {
            SerialMon.println("MPU6050 not found.");
            return false;
        }
        // SerialMon.println("MPU6050 found.");

        if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)MPU_INT))
        {
            SerialMon.println("MPU6050 interrupt pin invalid for wakeup.");
            return false;
        }
        // SerialMon.println("MPU6050 interrupt pin valid.");

        mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
        mpu.setMotionDetectionThreshold(8); //0-255, ideal range seems to be between 7 and 12.
        mpu.setMotionDetectionDuration(200); //1 time unit is 10ms, so 2 seconds is 200 units.
        mpu.setInterruptPinLatch(false); //Testing with auto interrupt clear, should be ok.
        mpu.setInterruptPinPolarity(false);
        mpu.setMotionInterrupt(true);

        esp_sleep_enable_ext0_wakeup((gpio_num_t)MPU_INT, 1);

        setupFailed = false;

        //Clear any interrupts (library cannot take nullptr otherwise the program will crash).
        Clear();

        SerialMon.println("Successfully configured MPU6050.");
        return true;
    }

    static void Loop()
    {
        Clear();
    }

    static void Clear()
    {
        if (setupFailed)
            return;
        sensors_event_t event = {};
        mpu.getEvent(&event, &event, &event);
        mpu.getMotionInterruptStatus(); //This should clear the interrupt status from what I read.
    }
};

TwoWire Motion::i2c = TwoWire(0);
Adafruit_MPU6050 Motion::mpu;
bool Motion::setupFailed = true;
