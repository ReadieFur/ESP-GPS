#pragma once

// #include "Service/AService.hpp"
#include <esp_log.h>
#include <driver/uart.h>

#if CONFIG_LOG_TIMESTAMP_SOURCE_RTOS
#define LOGE(tag, format, ...) Logging::Log(ESP_LOG_ERROR, tag, LOG_FORMAT(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define LOGW(tag, format, ...) Logging::Log(ESP_LOG_WARN, tag, LOG_FORMAT(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define LOGI(tag, format, ...) Logging::Log(ESP_LOG_INFO, tag, LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define LOGD(tag, format, ...) Logging::Log(ESP_LOG_DEBUG, tag, LOG_FORMAT(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define LOGV(tag, format, ...) Logging::Log(ESP_LOG_VERBOSE, tag, LOG_FORMAT(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#else
#define LOGE(tag, format, ...) Logging::Log(ESP_LOG_ERROR, tag, LOG_SYSTEM_TIME_FORMAT(E, format), esp_log_system_timestamp(), tag, ##__VA_ARGS__)
#define LOGW(tag, format, ...) Logging::Log(ESP_LOG_WARN, tag, LOG_SYSTEM_TIME_FORMAT(W, format), esp_log_system_timestamp(), tag, ##__VA_ARGS__)
#define LOGI(tag, format, ...) Logging::Log(ESP_LOG_INFO, tag, LOG_SYSTEM_TIME_FORMAT(I, format), esp_log_system_timestamp(), tag, ##__VA_ARGS__)
#define LOGD(tag, format, ...) Logging::Log(ESP_LOG_DEBUG, tag, LOG_SYSTEM_TIME_FORMAT(D, format), esp_log_system_timestamp(), tag, ##__VA_ARGS__)
#define LOGV(tag, format, ...) Logging::Log(ESP_LOG_VERBOSE, tag, LOG_SYSTEM_TIME_FORMAT(V, format), esp_log_system_timestamp(), tag, ##__VA_ARGS__)
#endif

namespace ReadieFur::EspGps
{
    class Logging //: public Service::AService
    {
    public:
        static void Log(esp_log_level_t level, const char* tag, const char* format, ...)
        {
            if (level == ESP_LOG_NONE || level > esp_log_level_get(tag))
                return;

            va_list args;
            va_start(args, format);

            esp_log_writev(level, tag, format, args);

            va_end(args);
        }

        static int Write(const char *format, va_list arg)
        {
            //TODO: Move this to serial monitor and call that method instead.
            //Based on esp32-hal-uart.c::log_printfv
            
            static char preallocBuffer[64]; //Static local buffer (persists across all calls, is faster than alloc each time).
            char *temp = preallocBuffer; //Pointer to buffer.
            va_list args;
            uint32_t len;
            va_list copy;

            //Copy the argument list because vsnprintf modifies it.
            va_start(args, format);
            va_copy(copy, args);

            //Determine the length of the formatted string.
            len = vsnprintf(NULL, 0, format, copy);
            va_end(copy);

            //If the formatted string exceeds the size of the static buffer, allocate memory dynamically.
            if (len >= sizeof(preallocBuffer))
            {
                //Allocate enough memory for the string and null terminator.
                temp = (char*)malloc(len + 1);
                if (temp == NULL)
                {
                    va_end(args); //End the variadic arguments if malloc fails.
                    return 0; //Return if memory allocation failed.
                }
            }

            //Format the string into the buffer.
            vsnprintf(temp, len + 1, format, args);

            uart_write_bytes(0, temp, len); //STDOUT is typically UART0.

            //Free dynamically allocated memory if it was used.
            if (temp != preallocBuffer)
                free(temp);

            va_end(args); //End the variadic arguments.
        }
    };
};
