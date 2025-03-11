#pragma once

#include <windows.h>
#include <sstream>

// High-resolution timer class
class Timer
{
public:
    Timer()
    {
        QueryPerformanceFrequency(&frequency);
    }

    void Start()
    {
        QueryPerformanceCounter(&startTime);
    }

    double GetElapsedMicroseconds()
    {
        LARGE_INTEGER current_time;
        bool          test = QueryPerformanceCounter(&current_time);

        if (test == 0)
            DebugBreak();

        if (current_time.QuadPart < lastCallTime.QuadPart)
            DebugBreak();
        lastCallTime = current_time;

        LARGE_INTEGER test_frequency;
        QueryPerformanceFrequency(&test_frequency);
        if (test_frequency.QuadPart != frequency.QuadPart)
            DebugBreak();

        double out = (current_time.QuadPart - startTime.QuadPart) * 1'000'000.0 / frequency.QuadPart;
        //if (out < lastOut)
        //	DebugBreak();
        lastOut = out;

        return out;
    }

private:
    std::ostringstream g_out;

    LARGE_INTEGER frequency    = {};  // Ticks per second
    LARGE_INTEGER startTime    = {};  // Start timestamp
    LARGE_INTEGER lastCallTime = {};
    double        lastOut      = 0;
};
