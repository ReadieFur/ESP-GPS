#pragma once

#define MODEM_DTR                       25
#define MODEM_TX                        26
#define MODEM_RX                        27
//The modem boot pin needs to follow the startup sequence.
#define MODEM_PWRKEY                    4
//The modem power switch must be set to HIGH for the modem to supply power.
#define MODEM_POWERON                   12
#define MODEM_RING                      33
#define MODEM_RESET                     5

#define GPS_TX                          21
#define GPS_RX                          22
#define GPS_PPS                         23
#define GPS_WAKEUP                      19
#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif

#define MPU_INT                         33
#define MPU_SDA                         18
#define MPU_SCL                         32
