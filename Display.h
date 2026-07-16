#pragma once

#include <Windows.h>
#include <thread>
#include <atomic>

// Display to the output device/monitor.
//
//  This is the output sequencing of both eyes going to the Game window.
//	It's the destination SwapChain and will call Present for each eye.
//
//  Has a standalone Device so that the Game Device cannot interfere with
//	the output frames.
//
//  If new frames are not ready, for any reason, the last two frames are
//  output. These will be stale, but the key aspect is making sync.

class Display
{
public:
    Display();
    ~Display();
    void StartRefresh();
    void StopRefresh();

    void RefreshLoop();

private:
    std::thread*      mRefreshThread = nullptr;
    std::atomic<bool> mRefreshing    = false;
};
