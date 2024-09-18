#pragma once
#include "Config.h"

#ifdef BOARD_CUSTOM

#if defined(ESP32)
#include <hal/gpio_types.h>
#elif defined(ESP8266)
#include <Arduino.h>
#endif


//====CONFIG STARTS HERE====

#pragma region Modem
#define MODEM_BAUDRATE          9600                //The baud rate for communication with the modem.
#define MODEM_TX                D3                  //The pin that the modem TX line is connected to.
#define MODEM_RX                D2                  //The pin that the modem RX line is connected to.
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER      1024
#pragma endregion

#pragma region GPS
#define GPS_TX                  D7                  //The pin that the GPS TX line is connected to.
#define GPS_RX                  D6                  //The pin that the GPS RX line is connected to.
#pragma endregion

//====CONFIG ENDS HERE====

#endif
