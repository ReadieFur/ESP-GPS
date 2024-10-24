#pragma once

#include <Stream.h>
#include "Logging.hpp"
#include <HardwareSerial.h>

namespace ReadieFur::EspGps
{
    class DebugStream : public Stream
    {
    public:
        int available() override { return 0; }

        int read() override
        {
            return Serial.read();
        }

        int peek() override
        {
            return Serial.peek();
        }

        size_t write(uint8_t c) override
        {
            return WRITE(c);
        }

        size_t write(const uint8_t *buffer, size_t size) override
        {
            return PRINT(reinterpret_cast<const char*>(buffer), size);
        }
    };
};

ReadieFur::EspGps::DebugStream DbgStream;
