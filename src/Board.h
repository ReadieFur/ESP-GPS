#pragma once
#include "Config.h"
#include <Arduino.h>

//Tweaked from: https://raw.githubusercontent.com/Xinyuan-LilyGO/LilyGO-T-A76XX/main/examples/HttpClient/utilities.h
#define MODEM_DTR                       25
#define MODEM_TX                        26
#define MODEM_RX                        27
//The modem boot pin needs to follow the startup sequence.
#define MODEM_PWRKEY                    4
//The modem power switch must be set to HIGH for the modem to supply power.
#define MODEM_POWERON                   12
#define MODEM_RING                      33
#define MODEM_RESET                     5
#define SerialAT                        Serial1
#define GPS_TX                          21
#define GPS_RX                          22
#define GPS_PPS                         23
#define GPS_WAKEUP                      19
#define SerialGPS                       Serial2
#define BOARD_ADC                       35
#define BOARD_BAT_ADC                   35
//It is only available in V1.4 version. In other versions, IO36 is not connected.
#define BOARD_SOLAR_ADC                 36
#define SerialMon                       Serial
#define BATTERY_CRIT_VOLTAGE            3550        //Shutdown voltage (in millivolts).
#define BATTERY_LOW_VOLTAGE             3650        //Low voltage (in millivolts).
#define BATTERY_CHG_VOLTAGE_HIGH        3825        //Voltage when plugged in and fully charged (in millivolts).
#define BATTERY_CHG_VOLTAGE_LOW         300         //Voltage when plugged in with the battery disconnected (in millivolts).
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif
#define MPU_INT                         33
#define MPU_SDA                         18
#define MPU_SCL                         32
