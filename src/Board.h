#pragma once

#include <Arduino.h>

#define MODEM_TX                        1
#define MODEM_RX                        2
#define MODEM_UART                     Serial1
#define TINY_GSM_MODEM_A7670

#define MPU_INT                         5
#define MPU_SDA                         8
#define MPU_SCL                         9

// #define BATTERY_ADC                     4
// #define BATTERY_CRIT_VOLTAGE            3550        //Shutdown voltage (in millivolts).
// #define BATTERY_LOW_VOLTAGE             3650        //Low voltage (in millivolts).
// #define CHG_VOLTAGE_MIN                 1000        //The minimum voltage threshold to consider if the device is charging.
// #define NO_BATTERY_VOLTAGE              300         //For testing only, if the voltage is below this then consider the device to be plugged in.
