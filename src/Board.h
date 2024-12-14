#pragma once

#define MODEM_TX                        26
#define MODEM_RX                        27
#define MODEM_PWRKEY                    4
#define MODEM_POWERON                   12
#define MODEM_RESET                     5
#define MODEM_RESET_LEVEL               HIGH
#define MODEM_DTR                       25
#define TINY_GSM_MODEM_A7670

#define GPS_TX                          21
#define GPS_RX                          22
#define GPS_PPS                         23
#define GPS_WAKEUP                      19
#define GPS_SLEEP_LEVEL                 HIGH

#define MPU_INT                         33
#define MPU_SDA                         18
#define MPU_SCL                         32

#define BATTERY_ADC                     35
#define CHARGE_ADC                      36
#define BATTERY_CRIT_VOLTAGE            3550        //Shutdown voltage (in millivolts).
#define BATTERY_LOW_VOLTAGE             3650        //Low voltage (in millivolts).
#define CHG_VOLTAGE_MIN                 1000        //The minimum voltage threshold to consider if the device is charging.
#define NO_BATTERY_VOLTAGE              300         //For testing only, if the voltage is below this then consider the device to be plugged in.
