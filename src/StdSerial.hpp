#pragma once

//On ESP32 I dont have good luck with Serial and I have to instead use std::cout
#if defined(ESP32)
#include <Stream.h>
#include <iostream>
#include <string>

class StdSerial : public Stream
{
public:
    StdSerial(std::ostream& out, std::istream& in) : _out(out), _in(in) {}

    int available() override
    {
        return _in.peek() != EOF ? 1 : 0; // Simplified; always returns 1 if there's data to read
    }

    int read() override
    {
        int ch = _in.get();
        if (ch == EOF)
            return -1;
        return ch;
    }

    int peek() override
    {
        return _in.peek();
    }

    size_t readBytes(char* buffer, size_t length) override
    {
        _in.read(buffer, length);
        return _in.gcount();
    }

    size_t readBytesUntil(char terminator, char* buffer, size_t length)
    {
        size_t count = 0;
        while (count < length && _in.get(buffer[count]))
        {
            if (buffer[count] == terminator)
                break;
            count++;
        }
        return count;
    }

    void flush() override
    {
        _out.flush();
    }

    void print(const String& str)
    {
        _out << str.c_str();
    }

    void print(int value)
    {
        _out << value;
    }

    void print(double value)
    {
        _out << value;
    }

    void println()
    {
        _out << std::endl;
    }

    void println(const String& str)
    {
        _out << str.c_str() << std::endl;
    }

    void println(int value)
    {
        _out << value << std::endl;
    }

    void println(double value)
    {
        _out << value << std::endl;
    }

    String readString()
    {
        String result;
        char ch;
        while (_in.get(ch))
        {
            if (ch == '\n' || ch == '\r')
                break;
            result += ch;
        }
        return result;
    }

    String readStringUntil(char terminator)
    {
        String result;
        char ch;
        while (_in.get(ch))
        {
            if (ch == terminator)
                break;
            result += ch;
        }
        return result;
    }

    void setTimeout(unsigned long timeout)
    {
        _timeout = timeout;
    }

    unsigned long getTimeout(void)
    {
        return _timeout;
    }

    size_t write(uint8_t c) override
    {
        _out << static_cast<char>(c);
        return 1;
    }

    void begin(unsigned long baud)
    {
        Serial.begin(baud);
    }

private:
    std::ostream& _out;
    std::istream& _in;
    unsigned long _timeout = 1000; // Default timeout
};

StdSerial stdSerial(std::cout, std::cin);
#else
#include <HardwareSerial.h>
HardwareSerial stdSerial(0);
#endif
