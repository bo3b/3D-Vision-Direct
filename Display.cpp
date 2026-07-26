#include "Display.h"

#include "Globals.h"
#include "Utils.h"

#include <thread>

void LogStalls();

Display::Display()
{
}

Display::~Display()
{
}

//--------------------------------------------------------------------------------------
// Output Refresh thread:
//  Will alternate between the R/L eye buffers to create frame-sequential output.
//
//  The reason to have a separate thread and all this complexity is so that we can
//  have a very strict output that matches the very strict monitor timing refresh.
//--------------------------------------------------------------------------------------

void Display::StartRefresh()
{
    g_out << "  --> refresh_thread StartRefresh " << endlog;

    // Fire up the emitter with default timings.
    g_shutterGlasses.WakeEmitter();
    g_shutterGlasses.InitEmitter();

    // Enable LightBoost if possible.
    //g_shutterGlasses.GetCurrentResolution();
    //g_shutterGlasses.EnableLightBoost();

    g_out << "Display::StartRefresh - Shutter glasses woken and started. " << endlog;

    IDXGIDevice1* dxgi_device = nullptr;
    HR(g_GameDevice->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgi_device));
    {
        HR(dxgi_device->SetMaximumFrameLatency(g_bufferCount - 1));
    }
    dxgi_device->Release();

    // Display-owned coherent pair snapshot. Matches the handoff's layout so a
    // single CopyResource per L-R cycle takes both slices at once.
    D3D11_TEXTURE2D_DESC pair_desc;
    g_game_latest_LR->GetDesc(&pair_desc);
    HR(g_GameDevice->CreateTexture2D(&pair_desc, nullptr, &mDisplayPair));

    // Allocate the thread that will run the dual present
    mRefreshThread = new std::thread(&Display::RefreshLoop, this);

    //SetThreadPriority(mRefreshThread, THREAD_PRIORITY_TIME_CRITICAL);
}

void Display::StopRefresh()
{
    g_out << "  --> refresh_thread StopRefresh " << endlog;

    mRefreshing = false;
    mRefreshThread->join();
}

//--------------------------------------------------------------------------------------
// The actual routine to execute as its own thread.
//
//  Some deep investigation into the different available Present types and buffer counts
//  showed that using Present(1,0) is pretty much the only real option.  Present(0,0) will
//  immediately show the buffer, and in Exclusive mode it will show screen tearing because
//  it comes it at some time after the GPU is available.
//
//  Probably our best combination is g_bufferCount=3, with SetMaximumFrameLatency(2)
//  so that we have the one being shown in frontbuffer, and the two pending in the queue.
//  This can be longer to handle larger stalls, but in general should not be necessary to
//  go beyond 2 in the queue. Anything bigger than that is a loading stall in hundreds of ms.
//
//  This is also why there is not point in doing both eyes in the main loop.  It will
//  always stop down to whatever the last one that was queued, either half done or not.
//
//  This cannot work in a game as currently written- the single device for output means that
//  a slow rendering game might queue up 50ms of GPU work that we can't interrupt in any way.
//  If that happens, we cannot add to the queue, and we get an eye-swap.

void Display::RefreshLoop()
{
    g_out << " --> RefreshLoop Startup " << endlog;

    mRefreshing = true;

    // Fetch backbuffer for long term reference.
    ComPtr<ID3D11Texture2D> refresh_backbuffer;
    HR(g_GameSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(refresh_backbuffer.GetAddressOf())));

    while (mRefreshing)
    {
        // As soon as we unblock, presumably vblank, hit emitter
        g_shutterGlasses.ToggleEyes();

        // On the Left eye (start of a new L-R cycle), snapshot the game's handoff
        // into our own coherent pair. Both eyes then read from this snapshot, so
        // a mid-cycle game update to g_game_latest_LR can't split the pair.
        if (g_shutterGlasses.IsLeftEye())
            g_GameImmediateContext->CopyResource(mDisplayPair.Get(), g_game_latest_LR.Get());

        // Copy whichever eye is up next from our owned snapshot. Bracket with
        // GpuTimer to catch head-of-queue stalls — GPU delta of ~0.1ms means
        // clean, spikes mean this Copy is waiting behind game work in the
        // shared command stream.
        g_CopyTimer->Begin();
        g_GameImmediateContext->CopySubresourceRegion(refresh_backbuffer.Get(), 0, 0, 0, 0, mDisplayPair.Get(), g_shutterGlasses.IsLeftEye(), nullptr);
        g_CopyTimer->End();

        // Present(1,) so that when the queue is full we block until it's free.
        HR(g_GameSwapChain->Present(1, 0));

        LogStalls();
    }
}
//--------------------------------------------------------------------------------------


static UINT   lost_frames     = 0;
static double last_frame_time = 0;  // likely unnecessary with QPC in stats.

void LogStalls()
{
    double current_frame_time = g_Timer.GetElapsedMicroseconds();
    double elapsed_ms         = (current_frame_time - last_frame_time) / 1000.0f;
    {
        bool stall_1 = (elapsed_ms > 8.6f);
        bool stall_2 = (elapsed_ms > 16.9f);

        if (stall_1)
            g_out << "  frame refresh stall:  " << elapsed_ms << " ms" << (stall_2 ? "-- stall" : "") << endlog;
    }
    last_frame_time = current_frame_time;

    // Follow presentation details to know if we need eye swap
    DXGI_FRAME_STATISTICS stats;
    HRESULT               hr = g_GameSwapChain->GetFrameStatistics(&stats);
    if (FAILED(hr))
    {
        if (hr == DXGI_ERROR_FRAME_STATISTICS_DISJOINT)
            g_out << "  ---GetFrameStatistics disjoint error---  " << elapsed_ms << " ms" << endlog;
        else
            g_out << "  ---GetFrameStatistics error: " << std::hex << hr << std::dec << endlog;
        return;
    }

    // Valid stats, check for two failure modes:
    //
    // (1) Dropped frames — SyncRefreshCount vs PresentCount widening.
    //     PresentCount advances per Present() call, so this catches CPU-side
    //     stalls where our Present didn't get issued for a vblank.
    if ((stats.SyncRefreshCount - stats.PresentCount) != lost_frames)
    {
        g_out << "  ---Dropped Frame---  " << elapsed_ms << " ms" << endlog;
        g_DroppedFrames += 1;
    }
    lost_frames = stats.SyncRefreshCount - stats.PresentCount;

    // (2) Flip slips — SyncRefreshCount delta between iterations.
    //     Sync is the free-running vblank counter; normally advances by 1
    //     between our iterations (one vblank per Present). If Present blocked
    //     an extra vblank because head-of-queue GPU work slipped, we sample
    //     AFTER that extra vblank, so Sync advanced by 2 or more. This catches
    //     the slip at the Present-block boundary, before the DXGI counters
    //     have a chance to catch up (which they do by the time we sample).
    //     Comparing PresentRefreshCount to Sync won't work: by our sample
    //     time Present() has already unblocked, meaning the slip resolved.
    static UINT last_sync = 0;
    if (last_sync != 0)
    {
        UINT sync_delta = stats.SyncRefreshCount - last_sync;
        if (sync_delta > 1)
        {
            g_out << "  ---Flip slip---  vblank delta=" << sync_delta << "  " << elapsed_ms << " ms" << endlog;
            g_FlipSlips += (sync_delta - 1);  // one slip per skipped vblank
        }
    }
    last_sync = stats.SyncRefreshCount;
}
