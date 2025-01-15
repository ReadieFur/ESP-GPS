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
// #define CHARGE_ADC                      
#define BATTERY_DIV1                    10150.0     //Resistance in ohms of the first resistor in the voltage divider.
#define BATTERY_DIV2                    10230.0     //Resistance in ohms of the second resistor in the voltage divider.
#define BATTERY_CHG_VOLTAGE             4000        //Voltage (in millivolts) at which the battery is considered to be charging.
#define BATTERY_LOW_VOLTAGE             3800        //Low voltage (in millivolts).
#define BATTERY_CRIT_VOLTAGE            3700        //Shutdown voltage (in millivolts).
