#include "Display.h"

#include "Globals.h"
#include "Utils.h"

#include <thread>

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
    g_out << " --> refresh_thread StartRefresh " << endlog;

    // Allocate the thread that will run the dual present
    mRefreshThread = new std::thread(&Display::RefreshLoop, this);

    //SetThreadPriority(mRefreshThread, THREAD_PRIORITY_TIME_CRITICAL);
}

void Display::StopRefresh()
{
    g_out << " --> refresh_thread StopRefresh " << endlog;

    mRefreshing = false;
    mRefreshThread->join();
}

// The actual routine to execute as its own thread.

void Display::RefreshLoop()
{
    g_out << " --> RefreshLoop Startup " << endlog;

    mRefreshing = true;

    ComPtr<IDXGIOutput> refresh_output;
    HR(g_GameSwapChain->GetContainingOutput(&refresh_output));
    ComPtr<ID3D11Texture2D> refresh_backbuffer;
    HR(g_GameSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(refresh_backbuffer.GetAddressOf())));

    while (mRefreshing)
    {
        if (FAILED(refresh_output->WaitForVBlank()))
        {
            DebugBreak();
            continue;
        }

        HRESULT hr;
        {
            // At vBlank, we want to Present next frame.
            // Copy in the latest bits to backbuffer.
            g_GameImmediateContext->CopySubresourceRegion(refresh_backbuffer.Get(), 0, 0, 0, 0, g_game_latest_LR.Get(), eye::left, nullptr);
            hr = g_GameSwapChain->Present(1, 0);
            if (FAILED(hr))
                DebugBreak();
            g_GameImmediateContext->CopySubresourceRegion(refresh_backbuffer.Get(), 0, 0, 0, 0, g_game_latest_LR.Get(), eye::right, nullptr);
            hr = g_GameSwapChain->Present(1, 0);
            if (FAILED(hr))
                DebugBreak();
        }
    }
}

//// Vblank clock: maps a QPC time to an absolute refresh count. Published by the
//// presenter from GetFrameStatistics, consumed by the emitter metronome thread
//// to identify each vblank's parity. Extrapolates across occlusion, since the
//// monitor never stops scanning.
//struct VBlankAnchor
//{
//    LONGLONG sync_qpc     = 0;  // stats.SyncQPCTime: QPC of the vblank below.
//    UINT     sync_refresh = 0;  // stats.SyncRefreshCount at that QPC.
//    double   period_qpc   = 0;  // QPC ticks per refresh, measured from the stats.
//    bool     valid        = false;
//};
//VBlankAnchor g_vblank_anchor;
//std::mutex   g_vblank_anchor_mutex;
//
////--------------------------------------------------------------------------------------
//// Output Refresh thread:
////  Will alternate between the R/L eye buffers to create frame-sequential output.
////
////  The reason to have a separate thread and all this complexity is so that we can
////  have a very strict output that matches the very strict monitor timing refresh.
////--------------------------------------------------------------------------------------
//
//void refresh_thread(void)
//{
//    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
//
//    g_out << " --> refresh_thread Start  (windowed: " << g_windowed
//          << ", high_priority: " << g_high_priority
//          << ", frame_latency: " << g_frame_latency
//          << ", fence_gate: " << g_fence_gate << ")" << std::endl;
//    log();
//
//    ComPtr<ID3D11Device>        refresh_device;
//    ComPtr<ID3D11DeviceContext> refresh_context;
//    ComPtr<ID3D11Texture2D>     refresh_backbuffer;
//    ComPtr<ID3D12Device>        device12;
//    ComPtr<ID3D12CommandQueue>  queue12;
//
//    ComPtr<ID3D11Texture2D> handoff_share[HANDOFF_SLOTS];
//    ComPtr<IDXGIKeyedMutex> handoff_mutex[HANDOFF_SLOTS];
//    ComPtr<ID3D11Texture2D> local_pair;
//
//    // Presenter device. F3 path: an 11on12 wrapper over a D3D12 HIGH-priority
//    // command queue, so our copies and presents preempt the game's command-
//    // buffer bursts instead of queueing behind them (geo-11's
//    // high_priority_queue; the mechanism VR compositors use). d3d12.dll is
//    // loaded dynamically so the plain path has no new link dependency.
//    if (g_high_priority)
//    {
//        HMODULE                 d3d12    = LoadLibraryW(L"d3d12.dll");
//        PFN_D3D12_CREATE_DEVICE create12 = d3d12 ? (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice") : nullptr;
//        if (create12 && SUCCEEDED(create12(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12))))
//        {
//            D3D12_COMMAND_QUEUE_DESC queue_desc = {};
//            queue_desc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
//            queue_desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
//            HR(device12->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue12)));
//
//            IUnknown* queue_iface = queue12.Get();
//            HR(D3D11On12CreateDevice(device12.Get(), 0, nullptr, 0, &queue_iface, 1, 0, &refresh_device, &refresh_context, nullptr));
//            g_out << "   presenter on HIGH-priority D3D12 queue via 11on12" << std::endl;
//            log();
//        }
//        else
//        {
//            g_out << "   D3D12 unavailable, falling back to normal-priority D3D11" << std::endl;
//            log();
//        }
//    }
//    if (!refresh_device)
//    {
//        HR(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &refresh_device, nullptr, &refresh_context));
//        g_out << "   presenter on normal-priority D3D11 device" << std::endl;
//        log();
//    }
//
//    // Presents in flight (F11): 2 submits each present ~2 refreshes ahead of
//    // its scanout deadline (geo-11's zero-flicker default); 1 saves a refresh
//    // of latency but leaves only ~8ms of GPU slack.
//    ComPtr<IDXGIDevice1> dxgi_device;
//    HR(refresh_device.As(&dxgi_device));
//    HR(dxgi_device->SetMaximumFrameLatency(g_frame_latency));
//
//    // The presenter swapchain, geo-11 shape: BufferCount 3 (latency N needs
//    // N+1 buffers or flip model blocks on buffer starvation), FLIP_SEQUENTIAL
//    // so GetFrameStatistics reports real scanout refreshes, created via the
//    // device's own factory (required for the 11on12 path), and created
//    // WINDOWED - exclusive fullscreen is engaged afterward with a
//    // revalidating ResizeBuffers, matching geo-11's engage sequence exactly.
//    DXGI_SWAP_CHAIN_DESC desc = {};
//    gameSwapChain->GetDesc(&desc);
//    desc.BufferCount  = 3;
//    desc.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
//    desc.OutputWindow = g_hWnd;
//    desc.Windowed     = TRUE;
//
//    ComPtr<IDXGIAdapter> adapter;
//    HR(dxgi_device->GetAdapter(&adapter));
//    ComPtr<IDXGIFactory1> factory;
//    HR(adapter->GetParent(IID_PPV_ARGS(&factory)));
//    HR(factory->CreateSwapChain(refresh_device.Get(), &desc, &g_refresh_swapchain));
//    factory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);
//    g_out << "refresh_thread CreateSwapChain for output window. SwapChain: " << g_refresh_swapchain.GetAddressOf() << std::endl;
//    log();
//
//    if (!g_windowed)
//    {
//        HRESULT hr = g_refresh_swapchain->SetFullscreenState(TRUE, nullptr);
//        if (SUCCEEDED(hr))
//        {
//            hr = g_refresh_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
//            g_out << "   exclusive fullscreen engaged (ResizeBuffers: 0x" << std::hex << hr << std::dec << ")" << std::endl;
//        }
//        else
//        {
//            g_out << "   SetFullscreenState(TRUE) failed: 0x" << std::hex << hr << std::dec << ", staying windowed" << std::endl;
//        }
//        log();
//    }
//
//    // Backbuffer fetched once (after any fullscreen transition) and reused
//    // across every Present - validated for this flip-model chain.
//    HR(g_refresh_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(refresh_backbuffer.GetAddressOf())));
//
//    // Open the handoff ring on the presenter device, and create the local
//    // 2-slice pair: both presents of a pair always come from the same snapshot.
//    for (int i = 0; i < HANDOFF_SLOTS; i++)
//    {
//        HR(refresh_device->OpenSharedResource(g_handoff_handle[i], __uuidof(ID3D11Texture2D), (void**)&handoff_share[i]));
//        HR(handoff_share[i].As(&handoff_mutex[i]));
//    }
//    D3D11_TEXTURE2D_DESC pair_desc;
//    handoff_share[0]->GetDesc(&pair_desc);
//    pair_desc.MiscFlags = 0;
//    pair_desc.BindFlags = 0;
//    HR(refresh_device->CreateTexture2D(&pair_desc, nullptr, &local_pair));
//
//    // Presenter side of the handoff (geo-11's AdoptPair): drain the ring in
//    // order; the readiness fence gate (F9) refuses a pair whose GPU copy is
//    // still queued behind the game's frame - queuing our copy then would make
//    // this thread's whole queue (pair copy, backbuffer copy, Present) inherit
//    // that wait, drain the flip queue, and repeat one eye on screen: the
//    // sub-60 wobble under test. Sub-timed for the slip diagnostics.
//    UINT   read_index         = 0;
//    UINT64 last_adopted_fence = 0;
//    float  adopt_acquire_ms = 0, adopt_copy_ms = 0, adopt_release_ms = 0;
//
//    LARGE_INTEGER qpc_freq;
//    QueryPerformanceFrequency(&qpc_freq);
//    const double qpc_to_ms = 1000.0 / (double)qpc_freq.QuadPart;
//
//    auto adopt_pair = [&](DWORD timeout_ms) -> bool
//    {
//        int           slot = read_index % HANDOFF_SLOTS;
//        LARGE_INTEGER t0, t1, t2, t3;
//        QueryPerformanceCounter(&t0);
//
//        // Readiness gate, checked BEFORE AcquireSync ever runs: the acquire
//        // is not trustworthy as a non-blocking probe - on an 11on12 device it
//        // has been measured blocking 5-10ms even with a 0 timeout, waiting on
//        // the game's still-executing handoff copy. That is the presenter
//        // waiting on the game, the exact thing this architecture forbids
//        // (suspected root of the geo-11 sub-120fps wobble). FIFO fence values
//        // are sequential - one per delivered pair - so the next pair's value
//        // is known (last adopted + 1) without reading anything the game owns:
//        // this check is pure CPU, and once it passes, the pair's GPU copy is
//        // complete AND its ReleaseSync (issued just after the Signal) has
//        // happened, so the acquire below succeeds instantly.
//        if (g_fence_gate && g_handoff_fence && g_handoff_fence->GetCompletedValue() < last_adopted_fence + 1)
//        {
//            adopt_acquire_ms = 0;
//            adopt_copy_ms    = 0;
//            adopt_release_ms = 0;
//            return false;  // Pair not GPU-complete yet: reuse the current pair, never wait.
//        }
//
//        HRESULT hr = handoff_mutex[slot]->AcquireSync(1, timeout_ms);
//        QueryPerformanceCounter(&t1);
//        adopt_acquire_ms = (float)((t1.QuadPart - t0.QuadPart) * qpc_to_ms);
//        adopt_copy_ms    = 0;
//        adopt_release_ms = 0;
//        if (hr != S_OK)  // WAIT_TIMEOUT (no new pair) or failure: reuse the current pair.
//            return false;
//
//        // Post-acquire backstop (should never fire once the pre-check above
//        // passed - the fence is monotonic - but cheap defense in depth).
//        if (g_fence_gate && g_handoff_fence && g_handoff_fence->GetCompletedValue() < g_slot_fence_value[slot])
//        {
//            handoff_mutex[slot]->ReleaseSync(1);  // Hand it back untouched; retry next left vblank.
//            return false;
//        }
//
//        refresh_context->CopyResource(local_pair.Get(), handoff_share[slot].Get());
//        QueryPerformanceCounter(&t2);
//        adopt_copy_ms = (float)((t2.QuadPart - t1.QuadPart) * qpc_to_ms);
//
//        handoff_mutex[slot]->ReleaseSync(0);
//        QueryPerformanceCounter(&t3);
//        adopt_release_ms = (float)((t3.QuadPart - t2.QuadPart) * qpc_to_ms);
//
//        last_adopted_fence = g_slot_fence_value[slot];
//        read_index++;
//        return true;
//    };
//
//    // Take an initial copy of the eye pair, so the first presents show real
//    // data. Retry loop, because adopt_pair also refuses fence-incomplete pairs.
//    {
//        ULONGLONG seed_start = GetTickCount64();
//        bool      seeded     = false;
//        while (g_running && !(seeded = adopt_pair(2)) && GetTickCount64() - seed_start < 2000)
//            Sleep(1);
//        if (!seeded)
//        {
//            g_out << "** initial pair seed timed out" << std::endl;
//            log();
//        }
//    }
//
//    // Emitter metronome: the glasses are commanded from a dedicated vblank-paced
//    // thread, never from Present time. The emitter free-runs its own shutter
//    // timer and treats AA commands as a phase resync, without reliably honoring
//    // the eye identity byte- so the command stream must be a steady, strictly
//    // alternating cadence. Present-time commands jitter exactly when the
//    // presenter is recovering from a stall, and one badly timed burst re-phases
//    // the emitter into a persistent eye swap even though the on-screen images
//    // are correct. This thread also keeps commands flowing while the presenter
//    // is occluded, so the glasses neither drift nor hit the emitter's ~4s idle
//    // shutoff during alt-tab.
//    ComPtr<IDXGIOutput> refresh_output;
//    HR(g_refresh_swapchain->GetContainingOutput(&refresh_output));
//
//    std::thread emitter_thread([refresh_output]()
//                               {
//                                   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
//
//                                   while (g_running)
//                                   {
//                                       if (FAILED(refresh_output->WaitForVBlank()))
//                                       {
//                                           DebugBreak();
//                                           Sleep(8);
//                                           continue;
//                                       }
//
//                                       LARGE_INTEGER now;
//                                       QueryPerformanceCounter(&now);
//
//                                       VBlankAnchor anchor;
//                                       {
//                                           std::lock_guard<std::mutex> hold(g_vblank_anchor_mutex);
//                                           anchor = g_vblank_anchor;
//                                       }
//                                       if (!anchor.valid)
//                                           continue;  // Glasses coast on the emitter's internal timer until stats arrive.
//
//                                       // Index of the refresh that just began scanning out. Deriving it from
//                                       // the QPC anchor instead of counting wakeups keeps the parity exact
//                                       // even when this thread misses vblanks.
//                                       UINT refresh = anchor.sync_refresh + (UINT)((now.QuadPart - anchor.sync_qpc) / anchor.period_qpc + 0.5);
//                                       bool left    = (refresh & 1) == 0;
//
//                                       // g_eye_swap inverts the command relative to the image: the calibration
//                                       // for chains where the protocol's 'left' actually opens the right lens,
//                                       // or the content's L/R is reversed.
//                                       if (left != (g_eye_swap != 0))
//                                           g_shutterGlasses.SetLeftEye();
//                                       else
//                                           g_shutterGlasses.SetRightEye();
//                                   }
//                               });
//
//    // Parity-driven presenter.
//    //
//    // Eye identity is derived from the absolute refresh count of the vblank each
//    // Present lands on (even = left, odd = right), never
//    // from alternation. GetFrameStatistics is the ground truth for which refresh
//    // each present actually appeared on; the prediction is re-anchored from it
//    // every frame, so any stall degrades to a repeated eye for one frame instead
//    // of a persistent eye-swap.
//
//    struct present_record
//    {
//        UINT  present_count;
//        UINT  predicted_refresh;
//        bool  left;
//        bool  anchored;          // Predicted with a valid stats anchor.
//        float gap_ms;            // Since the previous Present returned (stats readback + loop overhead).
//        float adopt_ms;          // AcquireSync + fence gate + pair CopyResource submit.
//        float copy_ms;           // Backbuffer copy submit.
//        float present_ms;        // The Present call including its pacing block.
//        float adopt_acquire_ms;  // AcquireSync / CopyResource / ReleaseSync
//        float adopt_copy_ms;     // breakdown of adopt_ms; all zero on
//        float adopt_release_ms;  // right-presents (no adoption attempted).
//    };
//    present_record history[16] = {};
//
//    bool     stats_valid           = false;  // Anchor usable? False until first stats arrive, and after disjoint/occlusion.
//    UINT     anchor_present        = 0;      // stats.PresentCount at the anchor.
//    UINT     anchor_refresh        = 0;      // stats.PresentRefreshCount at the anchor.
//    UINT     presents_issued       = 0;      // GetLastPresentCount after our latest Present.
//    UINT     last_verified_present = 0;
//    bool     prev_left             = false;
//    int      startup_log           = 8;
//    LONGLONG prev_present_return   = 0;
//    bool     occluded              = false;
//
//    UINT     period_base_refresh = 0;  // First sync record since the last disjoint;
//    LONGLONG period_base_qpc     = 0;  // the vblank period is measured from here.
//
//    while (g_running)
//    {
//        // F6: injected stall, simulating this thread being preempted by the
//        // scheduler- the original eye-swap vector. Recovery should be a slip
//        // report, a repeated eye, and re-anchored parity; never a swap.
//        int stall_ms = g_stall_request_ms.exchange(0);
//        if (stall_ms)
//        {
//            g_out << "** F6: injected presenter stall of " << stall_ms << " ms" << std::endl;
//            log();
//            Sleep(stall_ms);
//        }
//
//        // Predict the refresh this Present will appear on. With frame latency 1,
//        // each present not yet reported by the stats occupies one refresh after
//        // the last reported one.
//        UINT predicted_refresh;
//        if (stats_valid)
//            predicted_refresh = anchor_refresh + (presents_issued - anchor_present) + 1;
//        else
//            predicted_refresh = presents_issued + 1;  // Free-running until stats arrive.
//
//        bool left = (predicted_refresh & 1) == 0;
//
//        if (stats_valid && left == prev_left && startup_log <= 0)
//        {
//            g_out << "== Repeating " << (left ? "L" : "R") << " eye to restore parity." << std::endl;
//            log();
//        }
//        prev_left = left;
//
//        // Pair boundary: only adopt a new game pair when the upcoming vblank is a
//        // left, so a pair can never be split across a boundary (no L/R images from
//        // different game frames). Try-acquire (0 timeout): if the game hasn't
//        // produced a new (GPU-complete) pair, adopt_pair returns false and the
//        // previous pair is reused - the presenter never blocks on the game.
//        LARGE_INTEGER t_start, t_adopt, t_copy, t_present;
//        QueryPerformanceCounter(&t_start);
//
//        adopt_acquire_ms = 0;
//        adopt_copy_ms    = 0;
//        adopt_release_ms = 0;
//        if (left)
//            adopt_pair(0);
//        QueryPerformanceCounter(&t_adopt);
//
//        refresh_context->CopySubresourceRegion(refresh_backbuffer.Get(), 0, 0, 0, 0, local_pair.Get(), left ? 0 : 1, nullptr);
//        QueryPerformanceCounter(&t_copy);
//
//        HRESULT hr = g_refresh_swapchain->Present(1, 0);
//        QueryPerformanceCounter(&t_present);
//
//        // Teardown: the main thread drops g_running, then releases exclusive
//        // fullscreen while this thread may be mid-loop. Swapchain calls fail
//        // during that transition - exit before treating any of them as fatal.
//        if (!g_running)
//            break;
//
//        if (hr == DXGI_STATUS_OCCLUDED)
//        {
//            // Alt-tab etc: nothing reached the screen and the stats go stale.
//            // Idle, and re-baseline parity when presents start landing again.
//            stats_valid         = false;
//            prev_present_return = 0;
//            if (!occluded)
//            {
//                occluded = true;
//                g_out << "** Presenter occluded. Parity re-baseline pending." << std::endl;
//                log();
//            }
//
//            // Keep draining the handoff while occluded, regardless of parity:
//            // nothing is displayed so pair-boundary discipline doesn't apply,
//            // but the FIFO game would otherwise block on a full ring whenever
//            // the frozen parity stopped on a right.
//            adopt_pair(0);
//
//            Sleep(5);
//            continue;
//        }
//        occluded = false;
//        HR(hr);
//
//        HR(g_refresh_swapchain->GetLastPresentCount(&presents_issued));
//        history[presents_issued % ARRAYSIZE(history)] = { presents_issued, predicted_refresh, left, stats_valid,
//                                                          prev_present_return ? (float)((t_start.QuadPart - prev_present_return) * qpc_to_ms) : 0.0f,
//                                                          (float)((t_adopt.QuadPart - t_start.QuadPart) * qpc_to_ms),
//                                                          (float)((t_copy.QuadPart - t_adopt.QuadPart) * qpc_to_ms),
//                                                          (float)((t_present.QuadPart - t_copy.QuadPart) * qpc_to_ms),
//                                                          adopt_acquire_ms, adopt_copy_ms, adopt_release_ms };
//        prev_present_return                           = t_present.QuadPart;
//
//        if (startup_log > 0)
//        {
//            g_out << "   present " << presents_issued << " (" << (left ? "L" : "R") << ")"
//                  << " predicted refresh: " << predicted_refresh
//                  << (stats_valid ? "" : "  [no anchor]") << std::endl;
//            log();
//            startup_log--;
//        }
//
//        // Verify against ground truth, and re-anchor the prediction.
//        DXGI_FRAME_STATISTICS stats = {};
//        hr                          = g_refresh_swapchain->GetFrameStatistics(&stats);
//        if (hr == DXGI_ERROR_FRAME_STATISTICS_DISJOINT)
//        {
//            stats_valid = false;
//
//            // The refresh counter or display mode may have changed, so the vblank
//            // clock is no longer trustworthy. The metronome goes quiet until a
//            // fresh anchor is published.
//            period_base_refresh = 0;
//            {
//                std::lock_guard<std::mutex> hold(g_vblank_anchor_mutex);
//                g_vblank_anchor.valid = false;
//            }
//            g_out << "** Frame statistics disjoint. Parity re-baseline pending." << std::endl;
//            log();
//        }
//        else if (SUCCEEDED(hr) && stats.SyncQPCTime.QuadPart != 0)
//        {
//            // Publish the vblank clock for the emitter metronome. The period is
//            // measured over the whole span since the last disjoint, so it converges
//            // on the true refresh period and stays accurate when extrapolated
//            // across long occlusions.
//            if (period_base_refresh == 0)
//            {
//                period_base_refresh = stats.SyncRefreshCount;
//                period_base_qpc     = stats.SyncQPCTime.QuadPart;
//            }
//            else if (stats.SyncRefreshCount > period_base_refresh)
//            {
//                std::lock_guard<std::mutex> hold(g_vblank_anchor_mutex);
//                g_vblank_anchor.sync_qpc     = stats.SyncQPCTime.QuadPart;
//                g_vblank_anchor.sync_refresh = stats.SyncRefreshCount;
//                g_vblank_anchor.period_qpc   = double(stats.SyncQPCTime.QuadPart - period_base_qpc) / (stats.SyncRefreshCount - period_base_refresh);
//                g_vblank_anchor.valid        = true;
//            }
//
//            if (stats.PresentCount == 0 || stats.PresentCount == last_verified_present)
//                continue;
//            last_verified_present = stats.PresentCount;
//
//            const present_record& rec = history[stats.PresentCount % ARRAYSIZE(history)];
//            if (rec.present_count == stats.PresentCount)
//            {
//                if (stats_valid && rec.anchored && stats.PresentRefreshCount != rec.predicted_refresh)
//                {
//                    g_out << "!! present " << rec.present_count << " (" << (rec.left ? "L" : "R") << ")"
//                          << " predicted refresh: " << rec.predicted_refresh
//                          << " landed on: " << stats.PresentRefreshCount
//                          << "  slip: " << (int)(stats.PresentRefreshCount - rec.predicted_refresh) << std::endl;
//
//                    // Which leg of the loop ate the frame? This present's
//                    // timings and the preceding one's (the miss is often the
//                    // prior iteration overrunning).
//                    const present_record& prev = history[(stats.PresentCount - 1) % ARRAYSIZE(history)];
//                    g_out << "    timings(ms) this: gap=" << rec.gap_ms << " adopt=" << rec.adopt_ms
//                          << " (acq=" << rec.adopt_acquire_ms << " copy=" << rec.adopt_copy_ms << " rel=" << rec.adopt_release_ms << ")"
//                          << " copy=" << rec.copy_ms << " present=" << rec.present_ms << std::endl;
//                    if (prev.present_count == stats.PresentCount - 1)
//                        g_out << "    timings(ms) prev: gap=" << prev.gap_ms << " adopt=" << prev.adopt_ms
//                              << " (acq=" << prev.adopt_acquire_ms << " copy=" << prev.adopt_copy_ms << " rel=" << prev.adopt_release_ms << ")"
//                              << " copy=" << prev.copy_ms << " present=" << prev.present_ms << std::endl;
//                    log();
//                }
//                if (!stats_valid)
//                {
//                    g_out << "== Parity anchor: present " << stats.PresentCount
//                          << " on refresh " << stats.PresentRefreshCount
//                          << "  QPC: " << stats.SyncQPCTime.QuadPart << std::endl;
//                    log();
//                }
//                anchor_present = stats.PresentCount;
//                anchor_refresh = stats.PresentRefreshCount;
//                stats_valid    = true;
//            }
//        }
//    }
//    g_out << " refresh thread loop exit-> " << std::endl;
//    log();
//
//    emitter_thread.join();
//
//    local_pair.Reset();
//    for (int i = 0; i < HANDOFF_SLOTS; i++)
//    {
//        handoff_mutex[i].Reset();
//        handoff_share[i].Reset();
//    }
//
//    // Not legal to do from this thread. Hangs.
//    // SetFullscreenState(false) is issued by the main thread (restart_presenter)
//    // BEFORE this thread is joined - the validated ordering.
//    log();
//
//    // All components are ComPtr and will automatically be disposed.
//    g_out << " <-- refresh_thread Exit" << std::endl;
//    log();
//}
