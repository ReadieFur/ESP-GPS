#pragma once

#include <Arduino.h>

#if defined(ESP32)
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>
#endif

#define nameof(name) #name
