#pragma once

#include <Arduino.h>
#include <type_traits>

template <typename T = void>
class TimedLoop
{
private:
    typedef T(*TCallback)();
    ulong _lastRun = 0;

public:
    ulong RunIntervalMs;
    TCallback Callback;

    TimedLoop(ulong runIntervalMs, TCallback callback = nullptr)
    : RunIntervalMs(runIntervalMs), Callback(callback)
    {}

    //TODO: Handle default return value.
    T Loop()
    {
        if (Callback == nullptr)
            return;

        ulong now = millis();
        if (now - _lastRun < RunIntervalMs)
            return;

        if constexpr (!std::is_same<T, void>::value)
        {
            T retval = Callback();
            _lastRun = now;
            return retval;
        }
        else
        {
            Callback();
            _lastRun = now;
        }
    }
};
