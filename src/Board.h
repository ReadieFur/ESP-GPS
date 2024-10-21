#pragma once
#include "Config.h"
#ifndef BOARD_CUSTOM
#include <Arduino.h>

#pragma region LilyGO-T-A76XX
//Tweaked from: https://raw.githubusercontent.com/Xinyuan-LilyGO/LilyGO-T-A76XX/main/examples/HttpClient/utilities.h
#if defined(BOARD_LILYGO_T_A7670E) || defined(BOARD_LILYGO_T_A7670G) || defined(BOARD_LILYGO_T_A7670SA)
#define MODEM_BAUDRATE                  115200
#define MODEM_DTR                       25
#define MODEM_TX                        26
#define MODEM_RX                        27
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    4
#define BOARD_ADC                       35
//The modem power switch must be set to HIGH for the modem to supply power.
#define BOARD_POWERON                   12
#define MODEM_RING                      33
#define MODEM_RESET                     5
#define BOARD_MISO                      2
#define BOARD_MOSI                      15
#define BOARD_SCK                       14
#define BOARD_SD_CS                     13
#define BOARD_BAT_ADC                   35
#define MODEM_RESET_LEVEL               HIGH
#define SerialAT                        Serial1
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif
//It is only available in V1.4 version. In other versions, IO36 is not connected.
#define BOARD_SOLAR_ADC                 36
#if defined(BOARD_LILYGO_T_A7670E) || defined(BOARD_LILYGO_T_A7670SA)
//My assumption is that the E and SA variants use pin 127 like the other models but the documentation doesn't specify this anywhere so these will remain untested and unverified.
#define MODEM_GPS_ENABLE_GPIO           -1
// #define MODEM_GPS_ENABLE_GPIO           127
// #error "The A7670 E and SA variants are not supported yet."
#elif defined(BOARD_LILYGO_T_A7670G)
//According to the examples documentation, all other boards I believe should use the internal GPS method with the A7670G being the only exemption.
#define GPS_TX                          21
#define GPS_RX                          22
#define GPS_PPS                         23
#define GPS_WAKEUP                      19
#endif

#elif defined(BOARD_LILYGO_T_CALL_A7670E_V1_0) || defined(BOARD_LILYGO_T_CALL_A7670SA_V1_0)
//The G variant of this board does not support GPS.
#define MODEM_BAUDRATE                  115200
#define MODEM_DTR                       14
#define MODEM_TX                        26
#define MODEM_RX                        25
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    4
#define BOARD_LED                       12
//There is no modem power control, the LED Pin is used as a power indicator here.
#define BOARD_POWERON                   BOARD_LED
#define MODEM_RING                      13
#define MODEM_RESET                     27
#define MODEM_RESET_LEVEL               LOW
#define SerialAT                        Serial1
#define MODEM_GPS_ENABLE_GPIO           -1
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif

#elif defined(BOARD_LILYGO_T_CALL_A7670E_V1_1) || defined(BOARD_LILYGO_T_CALL_A7670SA_V1_1)
//The G variant of this board does not support GPS.
#define MODEM_BAUDRATE                  115200
#define MODEM_DTR                       32
#define MODEM_TX                        27
#define MODEM_RX                        26
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    4
#define BOARD_LED                       13
//There is no modem power control, the LED Pin is used as a power indicator here.
#define BOARD_POWERON                   BOARD_LED
#define MODEM_RING                      33
#define MODEM_RESET                     5
#define MODEM_RESET_LEVEL               LOW
#define SerialAT                        Serial1
#define MODEM_GPS_ENABLE_GPIO           -1
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif

#elif defined(BOARD_LILYGO_T_SIM767XG_S3)
#define MODEM_BAUDRATE                  115200
#define MODEM_DTR                       9
#define MODEM_TX                        11
#define MODEM_RX                        10
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    18
#define BOARD_LED                       12
//There is no modem power control, the LED Pin is used as a power indicator here.
#define BOARD_POWERON                   BOARD_LED
#define MODEM_RING                      3
#define MODEM_RESET                     17
#define MODEM_RESET_LEVEL               LOW
#define SerialAT                        Serial1
#define BOARD_BAT_ADC                   4
#define BOARD_SOLAR_ADC                 5
#define BOARD_MISO                      47
#define BOARD_MOSI                      14
#define BOARD_SCK                       21
#define BOARD_SD_CS                     13
#ifndef TINY_GSM_MODEM_SIM7672
#define TINY_GSM_MODEM_SIM7672
#endif
#define MODEM_GPS_ENABLE_GPIO           4

#elif defined(BOARD_LILYGO_T_A7608X)
#define MODEM_BAUDRATE                  115200
#define MODEM_DTR                       25
#define MODEM_TX                        26
#define MODEM_RX                        27
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    4
#define BOARD_BAT_ADC                   35
//The modem power switch must be set to HIGH for the modem to supply power.
#define BOARD_POWERON                   12         //T-A7608-V2 is onboard led.
#define MODEM_RING                      33
#define MODEM_RESET                     5          //T-A7608-V2 no connection.
#define BOARD_MISO                      2
#define BOARD_MOSI                      15
#define BOARD_SCK                       14
#define BOARD_SD_CS                     13
#define MODEM_RESET_LEVEL               HIGH
#define SerialAT                        Serial1
#ifndef TINY_GSM_MODEM_A7608
#define TINY_GSM_MODEM_A7608
#endif
//Only version v1.1 or V2  has solar adc pin.
#define BOARD_SOLAR_ADC                 34
//127 is defined in GSM as the AUXVDD index.
#define MODEM_GPS_ENABLE_GPIO           127

#elif defined(BOARD_LILYGO_T_A7608X_S3)
#define MODEM_BAUDRATE                  115200
#define MODEM_DTR                       7
#define MODEM_TX                        17
#define MODEM_RX                        18
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    15
#define BOARD_BAT_ADC                   4
//The modem power switch must be set to HIGH for the modem to supply power.
// #define BOARD_POWERON                   12
#define MODEM_RING                      6
#define MODEM_RESET                     16
#define BOARD_MISO                      47
#define BOARD_MOSI                      14
#define BOARD_SCK                       21
#define BOARD_SD_CS                     13
#define MODEM_RESET_LEVEL               LOW
#define SerialAT                        Serial1
#ifndef TINY_GSM_MODEM_A7608
#define TINY_GSM_MODEM_A7608
#endif
//Only version v1.1 has solar adc pin.
#define BOARD_SOLAR_ADC                 3
//127 is defined in GSM as the AUXVDD index.
#define MODEM_GPS_ENABLE_GPIO           127

#elif defined(BOARD_LILYGO_T_A7608X_DC_S3)
#define MODEM_DTR                       5
#define MODEM_RX                        42
#define MODEM_TX                        41
//The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY                    38
#define MODEM_RING                      6
#define MODEM_RESET                     40
#define MODEM_RTS                       1
#define MODEM_CTS                       2
#define MODEM_RESET_LEVEL               LOW
#define SerialAT                        Serial1
#ifndef TINY_GSM_MODEM_A7608
#define TINY_GSM_MODEM_A7608
#endif
//127 is defined in GSM as the AUXVDD index.
#define MODEM_GPS_ENABLE_GPIO           127

#endif
#pragma endregion
#endif
