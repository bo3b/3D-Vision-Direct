#pragma once

#include <windows.h>
#include <sstream>
#include <d3d11_1.h>

#include "Utils.h"

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

    void SleepMicroseconds(int64_t microseconds)
    {
        LARGE_INTEGER start, current;

        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);

        int64_t target_ticks = (microseconds * frequency.QuadPart) / 1000000;

        do
        {
            QueryPerformanceCounter(&current);
        } while (current.QuadPart - start.QuadPart < target_ticks);
    }

private:
    std::ostringstream g_out;

    LARGE_INTEGER frequency    = {};  // Ticks per second
    LARGE_INTEGER startTime    = {};  // Start timestamp
    LARGE_INTEGER lastCallTime = {};
    double        lastOut      = 0;
};


//--------------------------------------------------------------------------------------
// GPU-side timer for a single bracket of draws.
//
// D3D11 timestamp queries sample the GPU's tick counter at the moment the GPU reaches
// a point in the command stream, so a real GPU-side duration needs a start/end pair
// plus a disjoint query that gives the tick rate. The result is not valid until the
// GPU has actually executed the work- polling immediately would stall the CPU.
//
// So we keep a 3-deep ring: issue this frame's queries, then read the ring entry that
// is ~2 frames old. LastMs() returns the previous reading (or -1.0f initially) if the
// oldest entry isn't ready yet, so callers can display it continuously.
//
// Note the API quirk: D3D11_QUERY_TIMESTAMP uses End() alone (no Begin), it just
// samples the tick counter at that point in the stream. TIMESTAMP_DISJOINT uses both.
//--------------------------------------------------------------------------------------
class GpuTimer
{
public:
    GpuTimer(ID3D11Device* device, ID3D11DeviceContext* context)
        : context_(context)
    {
        D3D11_QUERY_DESC qd = {};
        for each (Ring& r in rings_)
        {
            qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            HR(device->CreateQuery(&qd, &r.disjoint));
            qd.Query = D3D11_QUERY_TIMESTAMP;
            HR(device->CreateQuery(&qd, &r.start));
            HR(device->CreateQuery(&qd, &r.end));
        }
    }

    ~GpuTimer()
    {
        for each (Ring& r in rings_)
        {
            if (r.disjoint) r.disjoint->Release();
            if (r.start)    r.start->Release();
            if (r.end)      r.end->Release();
        }
    }

    // Open the bracket on the current ring slot.
    void Begin()
    {
        Ring& r = rings_[frame_idx_ % 3];
        context_->Begin(r.disjoint);
        context_->End(r.start);  // TIMESTAMP uses End (not Begin)- quirk of the API.
    }

    // Close the bracket and advance the ring.
    void End()
    {
        Ring& r = rings_[frame_idx_ % 3];
        context_->End(r.end);
        context_->End(r.disjoint);
        frame_idx_++;
    }

    // Poll the oldest ring slot. After End() has advanced frame_idx_, rings_[frame_idx_ % 3]
    // is the slot from ~2 frames ago- the oldest still-live entry, and the one most likely
    // to have completed on GPU. If not ready or disjoint, return the previous value so the
    // UI stays continuous.
    float LastMs()
    {
        Ring& r = rings_[frame_idx_ % 3];

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
        UINT64                              tstart;
        UINT64                              tend;
        if (context_->GetData(r.disjoint, &dj, sizeof(dj), 0) == S_OK && !dj.Disjoint &&
            context_->GetData(r.start,    &tstart, sizeof(tstart), 0) == S_OK &&
            context_->GetData(r.end,      &tend,   sizeof(tend),   0) == S_OK)
        {
            last_ms_ = float(tend - tstart) * 1000.0f / float(dj.Frequency);
        }
        return last_ms_;
    }

private:
    struct Ring
    {
        ID3D11Query* disjoint = nullptr;
        ID3D11Query* start    = nullptr;
        ID3D11Query* end      = nullptr;
    };

    ID3D11DeviceContext* context_ = nullptr;
    Ring                 rings_[3];
    UINT                 frame_idx_ = 0;
    float                last_ms_   = -1.0f;
};
