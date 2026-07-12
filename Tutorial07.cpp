//--------------------------------------------------------------------------------------
// File: Tutorial07.cpp
//
// Early reverse engineering of emitter: https://users.csc.calpoly.edu/~zwood/teaching/csc572/final11/rsomers/
// Test app for stereo viewing: https://github.com/bobsomers/3dvgl/tree/master
// Best example of emitter programming (Linux): https://sourceforge.net/p/libnvstusb/code/HEAD/tree/
// Conversion to Windows and GitHub: https://github.com/FlintEastwood/3DVisionActivator
// Original mtbs3d thread about hacking emitter: http://www.mtbs3d.com/phpBB/viewtopic.php?f=26&t=3130
// Best list of 3D Vision certified monitors: https://www.mtbs3d.com/phpbb/viewtopic.php?t=23314
//
// Originally the Tutorial07, now heavily modified to simply demonstrate
// the use of 3D Vision Direct Mode.
//
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729724.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//
// Bo3b: 5-8-17
//	This sample is derived from the Microsoft Tutorial07 sample in the
//	DirectX SDK.  The goal was to use as simple an example as possible
//	but still demonstrate using 3D Vision Direct Mode, using DX11.
//	The code was modified as little as possible, so the pieces demonstrated
//	by the Tutorial07 are still valid.
//
//	Some documention used for this include this whitepaper from NVidia.
//	http://www.nvidia.com/docs/io/40505/wp-05482-001_v01-final.pdf
//	Be wary of that document, the code is completely broken, and misleading
//	in a lot of aspects.  The broad brush strokes are correct.
//
//	The Stereoscopy pdf/presentation gives some good details on how it
//	all works, and a better structure for Direct Mode.
//	http://www.nvidia.com/content/PDF/GDC2011/Stereoscopy.pdf
//
//	This sample is old, but it includes some details on modifying the
//	projection matrix directly that were very helpful.
//	http://developer.download.nvidia.com/whitepapers/2011/StereoUnproject.zip
//
// Bo3b: 5-14-17
//	Updated to simplify the code for this code branch.
//	In this branch, the barest minimum of DX11 is used, to make the use of
//	3D Vision Direct Mode more clear.
//
// Bo3b: 2-25-25
//	This variant is exp_usb branch. The goal is to switch up the sample to
//	go direct to the nvidia usb emitter for a 3D Vision hardware setup that
//  will not require nvidia 3D Vision driver.
//	The sample already draws both eyes independently, instead of going to
//	DirectMode buffers, we'll do a Present for each eye, and tick the
//	emitter after each to swap eyes.
//
// Bo3b: 3-3-25
//   Adding some notes regarding the use of the NVidia emitter from USB directly.
//   Using the FlintEastwood/3DVisionActivator project code, I've got this working
//   with the NVidia emitter.  The emitter is a USB device, and the protocol is
//   documented in the libnvstusb project. So this code setup now works to drive
//	 the NVidia emitter directly, without the need for the NVidia 3D Vision driver.
//   This is running nicely, and by setting the monitor into LightBoost mode, I can
//   get the full 3D Vision experience without the NVidia driver *at all*.
//
//   However, it has the glitch of eye-swaps upon an alt-tab out of the app. Tried
//   lots of variants, but there doesn't seem to be any way to get it to stop that.
//   Using an independent thread did not solve it, which is surprising. It's not
//   clear how the NVidia driver avoids this, but I don't recall it ever having
//   eye-swaps. Non-zero chance we can fix this by having dual offscreen buffers
//   that are always the definitive 'eyes' being shown, but that's for the real
//   variant in geo-11.
//
//   There is some sort of internal timer to the emitter, that runs even if it is
//   not getting setEye commands.  It seems to be some mechanism to keep it swapping
//   eyes, even if there are glitches and frames are missed. So for example, we can
//   just hit SetLeftEye at the top of the loop, and it will auto-swap to right during
//   a given frame, even with no call. This runs for at least 6 frames, and maybe more.
//	 There is an auto-timeout of some form, where if does not get any AA setEye commands
//   it will stop running and turn off the bright green and infra-red to the glasses.
//
//   It's not at all clear why we get eye swaps, especially because I am sure to always
//   call SetLeftEye before drawing left eye data. So it also does not respect the
//   setEye command, and uses it as a way to resync it's timer to avoid drift, but
//   does not actually immediately switch eyes.  There is the $40 clear command, but
//   that also seems to do nothing. It does not restart the device in proper mode. I
//   removed the 'Read' commands, because I don't think we care about the front button
//   and scroll wheel at all. And the FlintEastwood repo was getting blue-screens
//   from that.
//
//   Best I can tell from testing is that during the alt-tab, we lose the second call
//   to Present(1,0), for the right eye. It's not logged, and there is no way for that
//   function to exit early. Somehow it's being killed, and not just returning an error,
//   because there are no error results. If we can figure out why it's killed, we can
//   presumably fix the eye-swaps.
//	 Don't know. I'm leaving it broken for now, because this is valuable even as it
//   stands, and I want to integrate this to geo-11.
//
//   Still does the bluescreen crash when accessed without waking. So it's not the
//   reading aspect, it's any access. We thus need a clean way to wake it before
//   using.
//
// Bo3b: 3-10-25
//   Got this fully working now, including enabling and disabling LightBoost when the
//   app is rendering. Some tricky little bits, but overall this is going to work well.
//   Solved the BSOD at WakeEmitter by opening the USB pipe, then closing it. This
//   clears whatever bad state was there, and allows the following open pipes to work.
//   Apparently it is also possible to time delay 3 seconds, maybe for the internal
//   emitter timeout, but this is faster and works.
//   Got the timing for LightBoost to work exactly right. Adding the +5 on back porch
//   works to enable LightBoost, but also requires the tweak to pixel clock to handle
//   that extra delay. Doing the NvAPI_DISP_RevertCustomDisplayTrial works, and seems
//   to match NVidia 3D Vision behavior. This seems superior to requiring a specific
//   manual or external resolution profile, because we can do all this inline.
//
//  Adding new monitor, the early g-sync prototype that was a board to install in
//  I think a PG248Q. A 1080p monitor. With the board installed it shows as:
//  NVIDIA G-SYNC 241910(G-SYNC Capable). And importantly, the ID: NVD_FFFE.
//  Changing the maxpixels by +5 also enables 3D mode as shown in the OSD, and
//  the screen goes brighter. It does not properly return to non-3D mode, but I
//  think LightBoost is on.
//  I don't know what the shutter glasses timing should be for this monitor, but
//  it's very likely to be the same as the PG248Q, which appears to be the same
//  as the PG278QR. When tested, it seems to sync properly.
//
// Bo3b: 3-30-25
//   Lots of experiments for trying to sync properly using different Present variants.
//   None of them work. The documentation does not appear to match what the OS actually
//   forces. For example, if we force tearing off in both the Device, and at Present
//   flags- the OS still forces tearing for a Present(0,0) call. This is- useless.
//   Present(2,0) does not work either, the synchronization is off. And in any case is
//   not what we need. Any extra buffers to the swapchain are useless, because you cannot
//   Draw into them, they are read only. You cannot fetch GetBuffer(2,..) and have it work.
//   Also useless. I conclude the only thing that actually works is Present(1,0).
//
//   With that in mind, trying a new tack of using ShareSurfaces to get our buffers to a
//   different thread, where we can sync to the monitor with Present(1,0) for each eye.
//   An alternate thread cannot Present using the main Device, because DX11 is not thread
//   safe and generates multi-thread corruption errors in the debug layer.
//   So we will create an alternate swap chain for the thread output, which will simply
//   Present both buffers in an alternating fashion.
//
// Claude: 7-6-26
//   Parity-driven presenter to fix eye swaps on stalls. Eye identity is now derived
//   from the absolute refresh count of the vblank each Present lands on (even = left,
//   odd = right), never from loop alternation. F2 inverts the glasses command
//   relative to the presented image (flipping both would self-cancel). The refresh
//   swapchain is flip-model (FLIP_SEQUENTIAL, 2 buffers) with frame latency 1, so
//   GetFrameStatistics reports the real scanout refresh for each present, and the
//   next present's landing refresh is predictable. The prediction is verified and
//   re-anchored from the stats every frame, so a stall of any length degrades to a
//   repeated eye for one frame instead of a persistent eye-swap. The latch of a new
//   eye pair only happens before a left-eye present (pair boundary), via try_lock,
//   so the presenter never blocks on the render thread and never splits a pair.
//
// Claude: 7-7-26
//   The glasses are commanded from a dedicated vblank metronome thread, never from
//   Present time. The emitter free-runs its shutter timer and treats AA commands as
//   a phase resync without reliably honoring the eye identity byte, so jittery
//   present-time commands during stall recovery re-phased it into a persistent swap
//   even though the on-screen images had recovered correctly. The metronome wakes on
//   IDXGIOutput::WaitForVBlank, derives the vblank's absolute refresh index (and so
//   its parity) from a QPC anchor published out of the frame statistics, and sends a
//   strictly alternating command each vblank. It extrapolates across occlusion, so
//   alt-tab neither drifts the glasses nor lets them hit the emitter's idle shutoff.
//
// Claude: 7-12-26
//   geo-11 handoff architecture ported back here to reproduce (and dissect) the
//   Witcher3 "wobble": sustained slips in engaged exclusive fullscreen whenever the
//   game runs GPU-heavy at sub-60 fps (~25ms command-buffer bursts; capping the frame
//   rate does not shorten the bursts, and neither the HIGH-priority presenter queue
//   nor the content-readiness fence cured it in the real game). The eye handoff is
//   now geo-11's exactly: a 2-slot FIFO ring of 2-slice keyed-mutex array textures
//   (game: AcquireSync(0)->copy->Signal fence->ReleaseSync(1); presenter:
//   AcquireSync(1,0)->fence gate->copy to a local pair->ReleaseSync(0)), presenter
//   swapchain with BufferCount 3, and the geo-11 fullscreen engage sequence (create
//   windowed, SetFullscreenState(TRUE), revalidating ResizeBuffers).
//   Every suspect is now a live knob:
//     F3  - presenter device on a HIGH-priority D3D12 queue via 11on12 (restarts presenter)
//     F4  - exclusive fullscreen <-> windowed (restarts presenter)
//     F5  - game fps target: 120 / 60 / 40 / 30 / 58 / 20
//     F6  - one-shot random presenter stall (original eye-swap vector)
//     F7  - judder bar
//     F8  - GPU load per eye: 0 / 8k / 16k / 32k / 64k FMA iterations; the measured
//           GPU frame time is logged every ~2s via timestamp queries, so the load
//           can be dialed to match a real game's burst (~25ms = Witcher3 at max)
//     F9  - content-readiness fence gate on/off (A/B the geo-11 fix candidate)
//     F11 - presenter frame latency 1 <-> 2 (restarts presenter)
//
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>    // ID3D11Fence / ID3D11Device5 / ID3D11DeviceContext4 for the handoff readiness fence.
#include <d3d12.h>      // HIGH-priority presenter queue (F3), via 11on12.
#include <d3d11on12.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <thread>
#include <dwmapi.h>
#include <wrl/client.h>
#include <mutex>
#include <atomic>

#include "nvapi.h"

#include "Timer.h"
#include "ShutterGlasses.h"
#include "resource.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT          init_windows(HINSTANCE hInstance, int nCmdShow);
void             start_glasses();
void             enable_lightboost();
HRESULT          init_dx11();
void             cleanup_device();
LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
void             render_frame();
void             copy_to_handoff();
void             restart_presenter();
void             refresh_thread(void);

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct simple_vertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
};

struct shared_CB
{
    XMMATRIX mWorld;
    XMMATRIX mView;
    XMMATRIX mProjection;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE g_hInst       = nullptr;
HWND      g_hWnd        = nullptr;
HWND      g_hidden_hWnd = nullptr;
bool      g_windowed    = true;

ID3D11Device*          g_pd3dDevice        = nullptr;
ID3D11DeviceContext*   g_pImmediateContext = nullptr;
IDXGISwapChain*        g_pSwapChain        = nullptr;
ComPtr<IDXGISwapChain> g_refresh_swapchain;

ID3D11Texture2D*        g_pDepthStencil     = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader*  g_pPixelShader  = nullptr;
ID3D11InputLayout*  g_pVertexLayout = nullptr;
ID3D11Buffer*       g_pVertexBuffer = nullptr;
ID3D11Buffer*       g_pIndexBuffer  = nullptr;

ID3D11Buffer* g_pSharedCB = nullptr;

XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

LONG g_ScreenWidth  = 1920;
LONG g_ScreenHeight = 1080;
UINT g_bufferCount  = 2;

Timer              g_Timer;
double             g_lastFrame = 0;
std::ostringstream g_out;

NvidiaShutterGlasses g_shutterGlasses;
std::atomic<bool>    g_running(false);
std::thread          g_renderThread;
std::atomic<UINT>    g_eye_swap(0);  // Inverts the glasses command relative to the presented image. F2 flips it.

// Degenerate-case test hooks.
std::atomic<int>  g_render_fps(120);      // Render thread target fps. F5 cycles 120/60/40/30/58/20.
std::atomic<int>  g_stall_request_ms(0);  // One-shot presenter stall. F6 injects a random 5-1000ms preemption.
std::atomic<bool> g_judder_bar(true);     // F7 toggles the sweeping judder test bar.
float             g_bar_x = 0;            // Bar position, computed once per game frame so both eyes match.

// Presenter configuration knobs (geo-11 [3DVGlasses] equivalents). F3 and F11
// restart the presenter thread to apply; F8/F9 take effect immediately.
std::atomic<int>   g_load_iterations(0);   // F8: FMA iterations/pixel of the load pass, per eye. 0 = off.
std::atomic<bool>  g_fence_gate(true);     // F9: refuse pairs whose GPU copy hasn't completed.
std::atomic<bool>  g_high_priority(false); // F3: presenter on a HIGH-priority D3D12 queue via 11on12.
std::atomic<int>   g_frame_latency(2);     // F11: presenter presents in flight (1 or 2).
std::atomic<float> g_gpu_frame_ms(0);      // Measured GPU time per game frame (timestamp queries).

// Per-eye render targets on the game device (not shared - the handoff below is
// the only cross-device surface, matching geo-11's fake-backbuffer -> handoff shape).
ComPtr<ID3D11Texture2D>        g_right_eye_tex;
ComPtr<ID3D11Texture2D>        g_left_eye_tex;
ComPtr<ID3D11RenderTargetView> g_right_eye_RTV;
ComPtr<ID3D11RenderTargetView> g_left_eye_RTV;

// geo-11 handoff, ported exactly: a 2-slot FIFO ring of 2-slice array textures
// (slice 0 = left, slice 1 = right) with keyed mutexes, created on the game
// device and opened by shared handle on the presenter device. One texture =
// one keyed mutex = the L/R pair is adopted atomically.
// Game side: AcquireSync(0, 150) -> two CopySubresourceRegion -> Signal fence
// -> Flush -> ReleaseSync(1). Presenter side: AcquireSync(1, 0) -> fence gate
// -> CopyResource to a local pair -> ReleaseSync(0).
const int               HANDOFF_SLOTS = 2;
ComPtr<ID3D11Texture2D> g_handoff_tex[HANDOFF_SLOTS];
ComPtr<IDXGIKeyedMutex> g_handoff_mutex[HANDOFF_SLOTS];
HANDLE                  g_handoff_handle[HANDOFF_SLOTS] = {};
UINT                    g_handoff_write_index           = 0;

// Content-readiness fence (geo-11's wobble fix candidate, F9 gates its use):
// the keyed mutex hands over protocol state at CPU speed, but the pair's
// pixels only exist once the game's GPU has executed the handoff copy - which
// queues behind the game's whole frame of render work. Signaled on the game's
// context after each slot's copy; the presenter refuses to adopt (and reuses
// the previous pair) until the slot's value shows complete.
ComPtr<ID3D11Fence>          g_handoff_fence;
ComPtr<ID3D11DeviceContext4> g_game_context4;
UINT64                       g_handoff_fence_value             = 0;
UINT64                       g_slot_fence_value[HANDOFF_SLOTS] = {};

// GPU load pass (F8): screen-covering cube with the dependent-FMA shader.
ID3D11PixelShader* g_pLoadPS = nullptr;
ID3D11Buffer*      g_pLoadCB = nullptr;

// GPU frame-time measurement: a small ring of timestamp query sets so results
// are polled ~4 frames later without ever stalling the game thread.
struct gpu_probe
{
    ComPtr<ID3D11Query> disjoint;
    ComPtr<ID3D11Query> t0;
    ComPtr<ID3D11Query> t1;
    bool                inflight = false;
};
gpu_probe g_probe[4];
int       g_probe_index = 0;

// Vblank clock: maps a QPC time to an absolute refresh count. Published by the
// presenter from GetFrameStatistics, consumed by the emitter metronome thread
// to identify each vblank's parity. Extrapolates across occlusion, since the
// monitor never stops scanning.
struct VBlankAnchor
{
    LONGLONG sync_qpc     = 0;  // stats.SyncQPCTime: QPC of the vblank below.
    UINT     sync_refresh = 0;  // stats.SyncRefreshCount at that QPC.
    double   period_qpc   = 0;  // QPC ticks per refresh, measured from the stats.
    bool     valid        = false;
};
VBlankAnchor g_vblank_anchor;
std::mutex   g_vblank_anchor_mutex;

//--------------------------------------------------------------------------------------
// Frank Luna style error checking for stuff that should never fail.
//--------------------------------------------------------------------------------------
void HR(
    HRESULT hresult)
{
    if (FAILED(hresult))
    {
        std::ostringstream error_log;

        error_log << __FILE__ << ", " << __LINE__ << ", HR: " << std::hex << hresult << std::dec << std::endl;
        OutputDebugStringA(error_log.str().c_str());

        DebugBreak();
        exit(hresult);
    }
}

//--------------------------------------------------------------------------------------
// Clean usage of ostringstream for output logging.
// So that the log is always cleared.
//--------------------------------------------------------------------------------------
void log()
{
    OutputDebugStringA(g_out.str().c_str());
    g_out.str("");
}

//--------------------------------------------------------------------------------------
// Stop and restart the presenter thread (config knobs that require a new device or
// swapchain: F3 high-priority queue, F4 fullscreen, F11 frame latency). Runs on the
// main thread - SetFullscreenState(false) must be issued from here BEFORE the join,
// or a presenter blocked inside Present() on a fullscreen transition deadlocks
// (the validated ordering from the exclusive-fullscreen commit).
//--------------------------------------------------------------------------------------
void restart_presenter()
{
    // g_running must drop BEFORE the fullscreen release: the transition takes
    // tens of ms, and a presenter still looping through it sees the swapchain
    // in a transitional state where GetLastPresentCount/Present fail. With
    // g_running already false, the presenter exits instead of treating those
    // as fatal.
    g_running = false;
    if (g_refresh_swapchain)
        g_refresh_swapchain.Get()->SetFullscreenState(false, nullptr);

    if (g_renderThread.joinable())
        g_renderThread.join();

    // Reset the handoff ring: the keyed-mutex key states live in the shared
    // resources and survive the presenter, so a slot published (key 1) but
    // never consumed would wedge the new presenter/game cursor agreement.
    // Reclaim any presenter-readable slots back to game-writable, and restart
    // both cursors at slot 0 (the new presenter starts its read cursor at 0).
    for (int i = 0; i < HANDOFF_SLOTS; i++)
    {
        if (g_handoff_mutex[i] && g_handoff_mutex[i]->AcquireSync(1, 0) == S_OK)
            g_handoff_mutex[i]->ReleaseSync(0);
    }
    g_handoff_write_index = 0;

    g_out << " presenter restarted (windowed: " << g_windowed
          << ", high_priority: " << g_high_priority
          << ", frame_latency: " << g_frame_latency << ")" << std::endl;
    log();

    g_running      = true;
    g_renderThread = std::thread(refresh_thread);
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Before we create DX11 and windows, enable LightBoost.
    //enable_lightboost();

    // Safely Wake and Initialize the timing of the emitter.
    start_glasses();

    if (FAILED(init_windows(hInstance, nCmdShow)))
        return 0;

    if (FAILED(init_dx11()))
    {
        cleanup_device();
        return 0;
    }

    // Start a rendering subthread, so that rendering is off the main app thread,
    // and thus UI things like dragging the window don't block drawing.
    // The thread raises its own priority (ABOVE_NORMAL, matching geo-11).
    g_running      = true;
    g_renderThread = std::thread(refresh_thread);

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            // Handle Esc key to exit
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                PostQuitMessage(0);
            }
            // Swap eyes: inverts the glasses command relative to the presented
            // image. Flipping both together would be self-cancelling (right lens
            // still opens on the right image), so the one-time absolute L/R
            // calibration has to break that agreement on the glasses side only.
            {
                static bool f2_was_down = false;
                bool        f2_down     = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
                if (f2_down && !f2_was_down)
                    g_eye_swap ^= 1;
                f2_was_down = f2_down;
            }
            // Cycle the render thread's target frame rate through degenerate
            // cases. 40/30 are the Witcher3-wobble rates, 58 a sustained
            // just-below-refresh mismatch, 20 a slideshow; parity must hold
            // at all of them.
            {
                static bool f5_was_down = false;
                bool        f5_down     = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
                if (f5_down && !f5_was_down)
                {
                    static const int rates[]     = { 120, 60, 40, 30, 58, 20 };
                    static int       rate_index  = 0;
                    rate_index   = (rate_index + 1) % ARRAYSIZE(rates);
                    g_render_fps = rates[rate_index];
                    g_out << "== F5: render thread target now " << rates[rate_index] << " fps" << std::endl;
                    log();
                }
                f5_was_down = f5_down;
            }
            // Cycle the GPU load pass: FMA iterations per pixel, per eye, on a
            // screen-covering cube. Dial it while watching the measured "GPU
            // frame" log line until it matches the game being simulated
            // (Witcher3 at max ~= 25ms).
            {
                static bool f8_was_down = false;
                bool        f8_down     = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
                if (f8_down && !f8_was_down)
                {
                    static const int loads[]    = { 0, 8000, 16000, 32000, 64000 };
                    static int       load_index = 0;
                    load_index        = (load_index + 1) % ARRAYSIZE(loads);
                    g_load_iterations = loads[load_index];
                    g_out << "== F8: GPU load now " << loads[load_index] << " iterations/pixel per eye" << std::endl;
                    log();
                }
                f8_was_down = f8_down;
            }
            // Toggle the content-readiness fence gate (the geo-11 wobble fix
            // candidate): off = adopt pairs as soon as the keyed mutex opens,
            // even if the game's GPU copy hasn't executed yet.
            {
                static bool f9_was_down = false;
                bool        f9_down     = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
                if (f9_down && !f9_was_down)
                {
                    g_fence_gate = !g_fence_gate;
                    g_out << "== F9: readiness fence gate now " << (g_fence_gate ? "ON" : "OFF") << std::endl;
                    log();
                }
                f9_was_down = f9_down;
            }
            // Toggle the presenter's HIGH-priority D3D12 queue (11on12).
            {
                static bool f3_was_down = false;
                bool        f3_down     = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
                if (f3_down && !f3_was_down)
                {
                    g_high_priority = !g_high_priority;
                    g_out << "== F3: presenter high-priority queue now " << (g_high_priority ? "ON" : "OFF") << std::endl;
                    log();
                    restart_presenter();
                }
                f3_was_down = f3_down;
            }
            // Toggle the presenter's frame latency between 1 and 2.
            {
                static bool f11_was_down = false;
                bool        f11_down     = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
                if (f11_down && !f11_was_down)
                {
                    g_frame_latency = (g_frame_latency == 2) ? 1 : 2;
                    g_out << "== F11: presenter frame latency now " << g_frame_latency << std::endl;
                    log();
                    restart_presenter();
                }
                f11_was_down = f11_down;
            }
            // Inject a random 5-1000ms stall into the presenter thread, to
            // simulate scheduler preemption- the original eye-swap vector.
            // QPC low bits at human keypress time are random enough here.
            {
                static bool f6_was_down = false;
                bool        f6_down     = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
                if (f6_down && !f6_was_down)
                {
                    LARGE_INTEGER seed;
                    QueryPerformanceCounter(&seed);
                    g_stall_request_ms = 5 + (int)(seed.QuadPart % 996);
                }
                f6_was_down = f6_down;
            }
            // Toggle the judder test bar.
            {
                static bool f7_was_down = false;
                bool        f7_down     = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
                if (f7_down && !f7_was_down)
                    g_judder_bar = !g_judder_bar;
                f7_was_down = f7_down;
            }
            // Fullscreen toggle: the presenter swapchain owns the display state,
            // so this restarts the presenter, which re-engages via the geo-11
            // sequence (create windowed, SetFullscreenState(TRUE), revalidating
            // ResizeBuffers).
            {
                static bool f4_was_down = false;
                bool        f4_down     = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
                if (f4_down && !f4_was_down)
                {
                    g_windowed = !g_windowed;
                    g_out << "== F4: windowed now " << g_windowed << std::endl;
                    log();
                    restart_presenter();
                }
                f4_was_down = f4_down;
            }
        }
        else
        {
            render_frame();  // both eyes
        }
    }

    // Cleanly stop drawing thread upon exit. Fullscreen must be dropped from
    // THIS thread before the join (same ordering as restart_presenter), and
    // g_running before that, so the presenter doesn't treat transition-state
    // swapchain failures as fatal.
    g_running = false;
    if (g_refresh_swapchain)
        g_refresh_swapchain->SetFullscreenState(false, nullptr);
    if (g_renderThread.joinable())
    {
        g_renderThread.join();  // Wait for it to cleanly exit.
    }

    cleanup_device();

    return (int)msg.wParam;
}

//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT init_windows(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = window_proc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = nullptr;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm       = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Create window for the output mode.
    // For fullscreen it is required to have WS_POPUP.
    g_hInst = hInstance;
    RECT rc = { 0, 0, g_ScreenWidth, g_ScreenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 7", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    //g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 7", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd)
        return E_FAIL;
    g_out << "Main refresh window created: " << g_hWnd << std::endl;
    log();

    ShowWindow(g_hWnd, nCmdShow);
    g_out << "Main refresh window shown." << std::endl;
    log();

    // And a secondary window for rendering to happen. We don't actually need this
    // here, but want to emulate a game injected operation.  This window is not
    // shown, but needed for the 'game' swapchain.  This is necessary for fullscreen
    // exclusive to work, because only a single swapchain can target the output window.

    g_hidden_hWnd = CreateWindow(L"TutorialWindowClass", L"Hidden", WS_BORDER, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hidden_hWnd)
        return E_FAIL;
    g_out << "Secondary hidden render window created: " << g_hidden_hWnd << std::endl;
    log();

    g_Timer.Start();
    g_out << std::fixed << std::setprecision(2);

    return S_OK;
}

void enable_lightboost()
{
    NvAPI_Status status;

    status = g_shutterGlasses.GetCurrentResolution();
    if (status != NVAPI_OK)
    {
        g_out << "!!! Fail !!!" << std::endl
              << " Unable to fetch current resolution and timing. " << std::endl
              << "!!! Fail !!!" << std::endl;
        log();
        exit(-1);
    }

    // Since we could get the resolution successfully, let's go ahead and enable
    // LightBoost.  We'll not error out if it fails to setup.
    status = g_shutterGlasses.EnableLightBoost();
    if (status != NVAPI_OK)
    {
        g_out << "!!! Fail !!!" << std::endl
              << " Unable to enable timing for LightBoost. " << std::endl
              << "!!! Fail !!!" << std::endl;
        log();
    }
}

void start_glasses()
{
    // Start timers and initialize the emitter timing values.
    g_shutterGlasses.WakeEmitter();
    g_shutterGlasses.InitEmitter();

    g_out << "Shutter glasses woken and started, " << std::endl;
    log();
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT compile_shader_from_file(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD shader_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    shader_flags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    shader_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* error_blob = nullptr;

    hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, shader_flags, 0, ppBlobOut, &error_blob);
    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
            error_blob->Release();
        }
        return hr;
    }
    if (error_blob)
        error_blob->Release();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT init_dx11()
{
    HRESULT   hr;
    ID3DBlob* shader_blob = nullptr;

    UINT create_device_flags = 0;
#ifdef _DEBUG
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC desc               = {};
    desc.BufferCount                        = g_bufferCount;  // Quad buffered stereo
    desc.BufferDesc.Width                   = g_ScreenWidth;
    desc.BufferDesc.Height                  = g_ScreenHeight;
    desc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator   = 120;  // Needs to be 120Hz for 3D Vision emitter
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow                       = g_hidden_hWnd;  // Main drawing specifically on hidden window
    desc.SampleDesc.Count                   = 1;
    desc.SampleDesc.Quality                 = 0;
    desc.Windowed                           = TRUE;
    desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

    // Create the simple DX11, Device, SwapChain, and Context.
    HR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_device_flags, nullptr, 0, D3D11_SDK_VERSION, &desc, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext));
    g_out << "init_dx11 CreateDeviceAndSwapChain for hidden render window. SwapChain: " << g_pSwapChain << std::endl;
    log();

    // Create the offscreen Texture2D for each eye that we will DrawIndexed into.
    // They need to be identical to the drawing backbuffer in size and color
    // format. Not shared: the cross-device surface is the keyed-mutex handoff
    // ring below, matching geo-11's fake-backbuffer -> handoff shape.

    {
        ComPtr<ID3D11Texture2D> drawing_backbuffer;
        HR(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(drawing_backbuffer.GetAddressOf())));

        D3D11_TEXTURE2D_DESC texture_desc;
        drawing_backbuffer->GetDesc(&texture_desc);

        HR(g_pd3dDevice->CreateTexture2D(&texture_desc, nullptr, &g_left_eye_tex));
        HR(g_pd3dDevice->CreateTexture2D(&texture_desc, nullptr, &g_right_eye_tex));

        HR(g_pd3dDevice->CreateRenderTargetView(g_left_eye_tex.Get(), nullptr, &g_left_eye_RTV));
        HR(g_pd3dDevice->CreateRenderTargetView(g_right_eye_tex.Get(), nullptr, &g_right_eye_RTV));

        // The geo-11 handoff ring: 2-slice keyed-mutex array textures, both
        // slots created up front (see the globals comment for the protocol).
        D3D11_TEXTURE2D_DESC handoff_desc = {};
        handoff_desc.Width               = texture_desc.Width;
        handoff_desc.Height              = texture_desc.Height;
        handoff_desc.MipLevels           = 1;
        handoff_desc.ArraySize           = 2;
        handoff_desc.Format              = texture_desc.Format;
        handoff_desc.SampleDesc.Count    = 1;
        handoff_desc.Usage               = D3D11_USAGE_DEFAULT;
        handoff_desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
        handoff_desc.MiscFlags           = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        for (int i = 0; i < HANDOFF_SLOTS; i++)
        {
            HR(g_pd3dDevice->CreateTexture2D(&handoff_desc, nullptr, &g_handoff_tex[i]));
            HR(g_handoff_tex[i].As(&g_handoff_mutex[i]));

            ComPtr<IDXGIResource> handoff_dxgi;
            HR(g_handoff_tex[i].As(&handoff_dxgi));
            HR(handoff_dxgi->GetSharedHandle(&g_handoff_handle[i]));
        }

        // Content-readiness fence (see the globals comment). Optional: a
        // runtime without ID3D11Fence just leaves the gate disabled.
        ComPtr<ID3D11Device5> device5;
        if (SUCCEEDED(g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device5), (void**)&device5)))
            device5->CreateFence(0, D3D11_FENCE_FLAG_NONE, __uuidof(ID3D11Fence), (void**)&g_handoff_fence);
        if (g_handoff_fence &&
            FAILED(g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext4), (void**)&g_game_context4)))
            g_handoff_fence.Reset();
        if (!g_handoff_fence)
        {
            g_out << "!! ID3D11Fence unavailable, readiness gate disabled" << std::endl;
            log();
        }

        // Timestamp query ring for the measured GPU-frame-time log.
        for (int i = 0; i < ARRAYSIZE(g_probe); i++)
        {
            D3D11_QUERY_DESC qd = {};
            qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            HR(g_pd3dDevice->CreateQuery(&qd, &g_probe[i].disjoint));
            qd.Query = D3D11_QUERY_TIMESTAMP;
            HR(g_pd3dDevice->CreateQuery(&qd, &g_probe[i].t0));
            HR(g_pd3dDevice->CreateQuery(&qd, &g_probe[i].t1));
        }

        g_out << "create_eye_textures + handoff ring for render device completed " << std::endl;
        log();
    }

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC desc_stencil = {};
    desc_stencil.Width                = g_ScreenWidth;
    desc_stencil.Height               = g_ScreenHeight;
    desc_stencil.MipLevels            = 1;
    desc_stencil.ArraySize            = 1;
    desc_stencil.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc_stencil.SampleDesc.Count     = 1;
    desc_stencil.SampleDesc.Quality   = 0;
    desc_stencil.Usage                = D3D11_USAGE_DEFAULT;
    desc_stencil.BindFlags            = D3D11_BIND_DEPTH_STENCIL;
    desc_stencil.CPUAccessFlags       = 0;
    desc_stencil.MiscFlags            = 0;
    hr                                = g_pd3dDevice->CreateTexture2D(&desc_stencil, nullptr, &g_pDepthStencil);
    if (FAILED(hr))
        return hr;

    // Create the depth stencil view
    //
    // This is not strictly necessary for our 3D, but is almost always used.
    D3D11_DEPTH_STENCIL_VIEW_DESC stencil_view_desc = {};
    stencil_view_desc.Format                        = desc_stencil.Format;
    stencil_view_desc.ViewDimension                 = D3D11_DSV_DIMENSION_TEXTURE2D;
    stencil_view_desc.Texture2D.MipSlice            = 0;

    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &stencil_view_desc, &g_pDepthStencilView);
    if (FAILED(hr))
        return hr;

    // This viewport is 2x the screen width.  The documentation directly contradicts
    // this usage and suggests per-eye specific ViewPorts, but this works correctly.
    D3D11_VIEWPORT vp;
    vp.Width    = (FLOAT)g_ScreenWidth;
    vp.Height   = (FLOAT)g_ScreenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Compile the vertex shader
    hr = compile_shader_from_file(L"Tutorial07.fx", "VS", "vs_4_0", &shader_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr))
    {
        shader_blob->Release();
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = ARRAYSIZE(layout);

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout(layout, num_elements, shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), &g_pVertexLayout);
    shader_blob->Release();
    if (FAILED(hr))
        return hr;

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // Compile the pixel shader
    hr = compile_shader_from_file(L"Tutorial07.fx", "PS", "ps_4_0", &shader_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &g_pPixelShader);
    shader_blob->Release();
    if (FAILED(hr))
        return hr;

    // Compile and create the GPU load pixel shader (F8), plus its b1 constant
    // buffer carrying the iteration count.
    hr = compile_shader_from_file(L"Tutorial07.fx", "PS_Load", "ps_4_0", &shader_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled (PS_Load).", L"Error", MB_OK);
        return hr;
    }
    hr = g_pd3dDevice->CreatePixelShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &g_pLoadPS);
    shader_blob->Release();
    if (FAILED(hr))
        return hr;

    {
        D3D11_BUFFER_DESC load_bd = {};
        load_bd.Usage             = D3D11_USAGE_DEFAULT;
        load_bd.ByteWidth         = 16;  // uint4 LoadParams
        load_bd.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        hr                        = g_pd3dDevice->CreateBuffer(&load_bd, nullptr, &g_pLoadCB);
        if (FAILED(hr))
            return hr;
    }

    // Create vertex buffer for the cube
    simple_vertex vertices[] = {
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },

        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage             = D3D11_USAGE_DEFAULT;
    bd.ByteWidth         = sizeof(simple_vertex) * 24;
    bd.BindFlags         = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags    = 0;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem                = vertices;

    hr = g_pd3dDevice->CreateBuffer(&bd, &init_data, &g_pVertexBuffer);
    if (FAILED(hr))
        return hr;

    // Set vertex buffer
    UINT stride = sizeof(simple_vertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // Create index buffer
    // Create vertex buffer
    WORD indices[] = {
        3, 1, 0,
        2, 1, 3,

        6, 4, 5,
        7, 4, 6,

        11, 9, 8,
        10, 9, 11,

        14, 12, 13,
        15, 12, 14,

        19, 17, 16,
        18, 17, 19,

        22, 20, 21,
        23, 20, 22
    };

    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(WORD) * 36;
    bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    init_data.pSysMem = indices;
    hr                = g_pd3dDevice->CreateBuffer(&bd, &init_data, &g_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffer
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(shared_CB);
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr                = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pSharedCB);
    if (FAILED(hr))
        return hr;

    // Initialize the world matrix
    g_World = XMMatrixIdentity();

    // Initialize the view matrix
    XMVECTOR eye = XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f);
    XMVECTOR at  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_View       = XMMatrixLookAtLH(eye, at, up);

    // Initialize the projection matrix
    //
    // For the projection matrix, the shaders know nothing about being in stereo,
    // so this needs to be only ScreenWidth, one per eye.
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)g_ScreenWidth / (float)g_ScreenHeight, 0.01f, 100.0f);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void cleanup_device()
{
    if (g_pSwapChain)
        g_pSwapChain->SetFullscreenState(FALSE, nullptr);

    if (g_pImmediateContext)
        g_pImmediateContext->ClearState();

    if (g_pLoadCB)
        g_pLoadCB->Release();
    if (g_pLoadPS)
        g_pLoadPS->Release();
    for (int i = 0; i < HANDOFF_SLOTS; i++)
    {
        g_handoff_mutex[i].Reset();
        g_handoff_tex[i].Reset();
    }
    g_game_context4.Reset();
    g_handoff_fence.Reset();
    for (int i = 0; i < ARRAYSIZE(g_probe); i++)
    {
        g_probe[i].disjoint.Reset();
        g_probe[i].t0.Reset();
        g_probe[i].t1.Reset();
    }

    if (g_pSharedCB)
        g_pSharedCB->Release();
    if (g_pVertexBuffer)
        g_pVertexBuffer->Release();
    if (g_pIndexBuffer)
        g_pIndexBuffer->Release();
    if (g_pVertexLayout)
        g_pVertexLayout->Release();

    if (g_pVertexShader)
        g_pVertexShader->Release();
    if (g_pPixelShader)
        g_pPixelShader->Release();
    if (g_pDepthStencil)
        g_pDepthStencil->Release();
    if (g_pDepthStencilView)
        g_pDepthStencilView->Release();

    if (g_pSwapChain)
        g_pSwapChain->Release();
    if (g_pImmediateContext)
        g_pImmediateContext->Release();
    if (g_pd3dDevice)
        g_pd3dDevice->Release();
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    switch (message)
    {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                g_out << "--> Deactivate  time: " << g_Timer.GetElapsedMicroseconds() / 1000.0f << " now: " << now.QuadPart << std::endl;
                log();
            }
            else
            {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                g_out << "<-- Activate    time: " << g_Timer.GetElapsedMicroseconds() / 1000.0f << " now: " << now.QuadPart << std::endl;
                log();
            }
            break;

        case WM_SIZE:
            if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
            {
                UINT width  = LOWORD(lParam);
                UINT height = HIWORD(lParam);

                g_out << "WM_SIZE event: " << width << "x" << height << std::endl;
                log();

                //if (g_pSwapChain)
                //{
                //    // Resize the swapchain buffers
                //    HRESULT hr = g_pSwapChain->ResizeBuffers(g_bufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
                //    if (FAILED(hr))
                //        DebugBreak();
                //}
            }
            break;

            // Note that this tutorial does not handle resizing (WM_SIZE) requests,
            // so we created the window without the resize border.

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Render current image, eye independent. eye_cb is this eye's projection setup;
// updated here (not by the caller) because the load pass below also writes b0.
//--------------------------------------------------------------------------------------
void draw_cube(bool rightEye, const shared_CB& eye_cb)
{
    // GPU load burst (F8): a screen-covering cube shaded with the long
    // dependent-FMA loop, drawn BEFORE the clear wipes it - pure GPU work with
    // no visual effect, simulating a heavy game's multi-ms command-buffer
    // burst (the Witcher3-at-max case this lab exists to reproduce).
    int load_iters = g_load_iterations;
    if (load_iters > 0)
    {
        ID3D11RenderTargetView* rtv_load[] = { rightEye ? g_right_eye_RTV.Get() : g_left_eye_RTV.Get() };
        g_pImmediateContext->OMSetRenderTargets(1, rtv_load, nullptr);

        shared_CB load_cb   = {};
        load_cb.mWorld      = XMMatrixTranspose(XMMatrixScaling(8.0f, 8.0f, 8.0f));
        load_cb.mView       = XMMatrixTranspose(g_View);
        load_cb.mProjection = XMMatrixTranspose(g_Projection);
        g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &load_cb, 0, 0);

        UINT load_params[4] = { (UINT)load_iters, 0, 0, 0 };
        g_pImmediateContext->UpdateSubresource(g_pLoadCB, 0, nullptr, load_params, 0, 0);

        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pSharedCB);
        g_pImmediateContext->PSSetShader(g_pLoadPS, nullptr, 0);
        g_pImmediateContext->PSSetConstantBuffers(1, 1, &g_pLoadCB);
        g_pImmediateContext->DrawIndexed(36, 0, 0);
    }

    //
    // Clear the buffer
    //
    if (rightEye)
        g_pImmediateContext->ClearRenderTargetView(g_right_eye_RTV.Get(), Colors::OliveDrab);
    else
        g_pImmediateContext->ClearRenderTargetView(g_left_eye_RTV.Get(), Colors::OliveDrab);
    //
    // Clear the depth buffer to 1.0 (max depth)
    //
    // Also done on a per-eye basis.
    //
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    assert(g_right_eye_RTV);  // This should not trigger!
    assert(g_left_eye_RTV);   // This should not trigger!

    // Set the RenderTargetView for the specific eye buffer
    ID3D11RenderTargetView* rtv_array[] = { rightEye ? g_right_eye_RTV.Get() : g_left_eye_RTV.Get() };
    if (rightEye)
        g_pImmediateContext->OMSetRenderTargets(1, rtv_array, nullptr);
    else
        g_pImmediateContext->OMSetRenderTargets(1, rtv_array, nullptr);

    //
    // Render the cube
    //
    // Projection matrix in g_pSharedCB determines eye view. Re-uploaded here
    // because the load pass above may have overwritten b0.
    //
    g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &eye_cb, 0, 0);
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pSharedCB);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->DrawIndexed(36, 0, 0);

    // Judder test bar (F7 toggles): the cube geometry squeezed into a thin
    // vertical bar, sweeping the width at constant speed. Drawn identically in
    // both eyes with the plain projection- zero parallax, so it sits at screen
    // depth. A repeated or dropped pair shows as a visible double-step in the
    // sweep, which the slowly rotating cube is too subtle to reveal.
    if (g_judder_bar)
    {
        shared_CB bar_cb   = {};
        bar_cb.mWorld      = XMMatrixTranspose(XMMatrixScaling(0.06f, 4.0f, 0.06f) * XMMatrixTranslation(g_bar_x, 1.0f, 0.0f));
        bar_cb.mView       = XMMatrixTranspose(g_View);
        bar_cb.mProjection = XMMatrixTranspose(g_Projection);
        g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &bar_cb, 0, 0);
        g_pImmediateContext->DrawIndexed(36, 0, 0);
    }
}

void sleep_microseconds(int64_t microseconds)
{
    LARGE_INTEGER frequency, start, current;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    int64_t target_ticks = (microseconds * frequency.QuadPart) / 1000000;

    do
    {
        QueryPerformanceCounter(&current);
    } while (current.QuadPart - start.QuadPart < target_ticks);
}

int64_t stall     = 0;
int     out_limit = 4;

//--------------------------------------------------------------------------------------
// Game side of the geo-11 handoff (CopyToHandoff port): publish the just-rendered
// L/R pair into the next FIFO slot under its keyed mutex, with the readiness
// fence signaled behind the copies. Never wedges the game thread: a 150ms cap
// means a hung presenter costs one dropped frame, not a hang.
//--------------------------------------------------------------------------------------
void copy_to_handoff()
{
    int slot = g_handoff_write_index % HANDOFF_SLOTS;

    HRESULT hr = g_handoff_mutex[slot]->AcquireSync(0, 150);
    if (hr != S_OK)
    {
        static ULONGLONG last_drop_log = 0;
        ULONGLONG        now           = GetTickCount64();
        if (now - last_drop_log >= 1000)
        {
            g_out << "!! game-side AcquireSync(0) slot " << slot << " failed/timed out: 0x" << std::hex << hr << std::dec << " (dropping frame)" << std::endl;
            log();
            last_drop_log = now;
        }
        return;
    }

    g_pImmediateContext->CopySubresourceRegion(g_handoff_tex[slot].Get(), 0, 0, 0, 0, g_left_eye_tex.Get(), 0, nullptr);
    g_pImmediateContext->CopySubresourceRegion(g_handoff_tex[slot].Get(), 1, 0, 0, 0, g_right_eye_tex.Get(), 0, nullptr);

    // Readiness signal: completes only when the GPU has actually executed the
    // copies above - which queue behind this frame's whole render burst.
    if (g_game_context4)
    {
        g_handoff_fence_value++;
        g_slot_fence_value[slot] = g_handoff_fence_value;
        g_game_context4->Signal(g_handoff_fence.Get(), g_handoff_fence_value);
    }

    // Ensure the copies are submitted to the GPU before handing the key over.
    g_pImmediateContext->Flush();

    g_handoff_mutex[slot]->ReleaseSync(1);
    g_handoff_write_index++;
}

//--------------------------------------------------------------------------------------
// Render a frame, both eyes. But do not Present.
//--------------------------------------------------------------------------------------
void render_frame()
{
    //
    // Rotate cube around the origin
    //
    g_World = XMMatrixRotationY(GetTickCount64() / 1000.0f);

    // Judder bar sweep: constant speed, wrapping every 2 seconds. Computed once
    // per game frame so both eyes place the bar identically.
    g_bar_x = (float)(fmod(GetTickCount64() / 1000.0, 2.0) / 2.0) * 9.0f - 4.5f;

    //
    // This now includes changing CBChangeOnResize each frame as well, because
    // we need to update the Projection matrix each frame, in case the user changes
    // the 3D settings.
    // The variable names are a bit misleading at present.
    //
    shared_CB cb                    = {};
    float     eye_convergence       = 35.0f;
    float     eye_separation        = 10.10f;
    float     separation_percentage = 0.22f;

    float separation  = eye_separation * separation_percentage / 100;
    float convergence = eye_separation * separation_percentage / 100 * eye_convergence;

    //stall += 10;
    //sleep_microseconds(stall);

    // Checking for possible fatal errors that could cause an eye swap situation.
    // Does not seem to ever hit exception handler, which is what we'd expect.
    try
    {
        // GPU frame-time probe: poll the result recorded ARRAYSIZE(g_probe)
        // frames ago (never stalls), then bracket this frame's submissions.
        gpu_probe& probe = g_probe[g_probe_index % ARRAYSIZE(g_probe)];
        if (probe.inflight)
        {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
            UINT64                              ts0, ts1;
            if (g_pImmediateContext->GetData(probe.disjoint.Get(), &dj, sizeof(dj), 0) == S_OK && !dj.Disjoint &&
                g_pImmediateContext->GetData(probe.t0.Get(), &ts0, sizeof(ts0), 0) == S_OK &&
                g_pImmediateContext->GetData(probe.t1.Get(), &ts1, sizeof(ts1), 0) == S_OK)
            {
                g_gpu_frame_ms = (float)((ts1 - ts0) * 1000.0 / dj.Frequency);
            }
            probe.inflight = false;
        }
        g_pImmediateContext->Begin(probe.disjoint.Get());
        g_pImmediateContext->End(probe.t0.Get());

        // <----------------------- Left Eye -------------------------------
        //
        // Drawing same object twice, once for each eye.
        // Eye specific setup is for the Projection matrix.
        // The _31 parameter is the X translation for the off center Projection.
        // The _41 parameter is the X translation after the perspective divide.
        // This sequence works to handle both convergence and separation hot keys properly.
        //
        {
            cb.mWorld = XMMatrixTranspose(g_World);
            cb.mView  = XMMatrixTranspose(g_View);

            cb.mProjection = g_Projection;
            cb.mProjection._31 -= separation;
            cb.mProjection._41 = convergence;
            cb.mProjection     = XMMatrixTranspose(cb.mProjection);

            draw_cube(false, cb);
        }

        // <----------------------- Right Eye -------------------------------
        //
        {
            cb.mWorld = XMMatrixTranspose(g_World);
            cb.mView  = XMMatrixTranspose(g_View);

            cb.mProjection = g_Projection;
            cb.mProjection._31 += separation;
            cb.mProjection._41 = -convergence;
            cb.mProjection     = XMMatrixTranspose(cb.mProjection);

            draw_cube(true, cb);
        }

        // Publish the pair to the presenter (keyed mutex + fence + Flush).
        copy_to_handoff();

        // Close this frame's GPU probe: the span covers both eyes' draws
        // (including the load pass) plus the handoff copies - the same GPU
        // work a real game's frame puts ahead of its handoff.
        g_pImmediateContext->End(probe.t1.Get());
        g_pImmediateContext->End(probe.disjoint.Get());
        probe.inflight = true;
        g_probe_index++;

        // Measured GPU frame time, logged every ~2 seconds so the F8 load can
        // be dialed against a real target (Witcher3 at max ~= 25ms).
        {
            static int frames_since_log = 0;
            if (++frames_since_log >= 240)
            {
                frames_since_log = 0;
                g_out << "== GPU frame: " << g_gpu_frame_ms << " ms  (load " << g_load_iterations
                      << " iters, game " << g_render_fps << " fps target)" << std::endl;
                log();
            }
        }

        double current_frame_time = g_Timer.GetElapsedMicroseconds();
        if (out_limit > 0)
        {
            g_out << "  full frame time:             " << (current_frame_time - g_lastFrame) / 1000.0f << " ms" << std::endl;
            log();

            out_limit--;
        }
        g_lastFrame = current_frame_time;

        // Frame-rate throttle for degenerate-case testing; F5 cycles the target.
        // Paced against a running deadline so the cadence is the actual
        // frame-to-frame rate (58 means 58), not render time plus a fixed sleep.
        // Sleep covers the bulk, a short spin lands the deadline precisely.
        {
            static int64_t next_deadline = 0;

            LARGE_INTEGER freq, now;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&now);

            int64_t period = freq.QuadPart / g_render_fps;
            if (now.QuadPart > next_deadline + period)
                next_deadline = now.QuadPart;  // Lost the cadence (stall, rate change): restart from now.
            next_deadline += period;

            int64_t remaining_us = (next_deadline - now.QuadPart) * 1000000 / freq.QuadPart;
            if (remaining_us > 3000)
                Sleep((DWORD)((remaining_us - 2000) / 1000));

            QueryPerformanceCounter(&now);
            if (now.QuadPart < next_deadline)
                sleep_microseconds((next_deadline - now.QuadPart) * 1000000 / freq.QuadPart);
        }
    }
    catch (const std::exception& e)
    {
        g_out << "!!!  render_frame exception: " << e.what() << std::endl;
        log();
        DebugBreak();
    }
    catch (...)
    {
        g_out << "!!!  Unknown render_frame exception: " << std::endl;
        log();
        DebugBreak();
    }
}

//--------------------------------------------------------------------------------------
// Output Refresh thread:
//  Will alternate between the R/L eye buffers to create frame-sequential output.
//
//  The reason to have a separate thread and all this complexity is so that we can
//  have a very strict output that matches the very strict monitor timing refresh.
//--------------------------------------------------------------------------------------

void refresh_thread(void)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    g_out << " --> refresh_thread Start  (windowed: " << g_windowed
          << ", high_priority: " << g_high_priority
          << ", frame_latency: " << g_frame_latency
          << ", fence_gate: " << g_fence_gate << ")" << std::endl;
    log();

    ComPtr<ID3D11Device>        refresh_device;
    ComPtr<ID3D11DeviceContext> refresh_context;
    ComPtr<ID3D11Texture2D>     refresh_backbuffer;
    ComPtr<ID3D12Device>        device12;
    ComPtr<ID3D12CommandQueue>  queue12;

    ComPtr<ID3D11Texture2D> handoff_share[HANDOFF_SLOTS];
    ComPtr<IDXGIKeyedMutex> handoff_mutex[HANDOFF_SLOTS];
    ComPtr<ID3D11Texture2D> local_pair;

    // Presenter device. F3 path: an 11on12 wrapper over a D3D12 HIGH-priority
    // command queue, so our copies and presents preempt the game's command-
    // buffer bursts instead of queueing behind them (geo-11's
    // high_priority_queue; the mechanism VR compositors use). d3d12.dll is
    // loaded dynamically so the plain path has no new link dependency.
    if (g_high_priority)
    {
        HMODULE                 d3d12    = LoadLibraryW(L"d3d12.dll");
        PFN_D3D12_CREATE_DEVICE create12 = d3d12 ? (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice") : nullptr;
        if (create12 && SUCCEEDED(create12(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12))))
        {
            D3D12_COMMAND_QUEUE_DESC queue_desc = {};
            queue_desc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queue_desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
            HR(device12->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue12)));

            IUnknown* queue_iface = queue12.Get();
            HR(D3D11On12CreateDevice(device12.Get(), 0, nullptr, 0, &queue_iface, 1, 0, &refresh_device, &refresh_context, nullptr));
            g_out << "   presenter on HIGH-priority D3D12 queue via 11on12" << std::endl;
            log();
        }
        else
        {
            g_out << "   D3D12 unavailable, falling back to normal-priority D3D11" << std::endl;
            log();
        }
    }
    if (!refresh_device)
    {
        HR(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &refresh_device, nullptr, &refresh_context));
        g_out << "   presenter on normal-priority D3D11 device" << std::endl;
        log();
    }

    // Presents in flight (F11): 2 submits each present ~2 refreshes ahead of
    // its scanout deadline (geo-11's zero-flicker default); 1 saves a refresh
    // of latency but leaves only ~8ms of GPU slack.
    ComPtr<IDXGIDevice1> dxgi_device;
    HR(refresh_device.As(&dxgi_device));
    HR(dxgi_device->SetMaximumFrameLatency(g_frame_latency));

    // The presenter swapchain, geo-11 shape: BufferCount 3 (latency N needs
    // N+1 buffers or flip model blocks on buffer starvation), FLIP_SEQUENTIAL
    // so GetFrameStatistics reports real scanout refreshes, created via the
    // device's own factory (required for the 11on12 path), and created
    // WINDOWED - exclusive fullscreen is engaged afterward with a
    // revalidating ResizeBuffers, matching geo-11's engage sequence exactly.
    DXGI_SWAP_CHAIN_DESC desc = {};
    g_pSwapChain->GetDesc(&desc);
    desc.BufferCount  = 3;
    desc.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.OutputWindow = g_hWnd;
    desc.Windowed     = TRUE;

    ComPtr<IDXGIAdapter> adapter;
    HR(dxgi_device->GetAdapter(&adapter));
    ComPtr<IDXGIFactory1> factory;
    HR(adapter->GetParent(IID_PPV_ARGS(&factory)));
    HR(factory->CreateSwapChain(refresh_device.Get(), &desc, &g_refresh_swapchain));
    factory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);
    g_out << "refresh_thread CreateSwapChain for output window. SwapChain: " << g_refresh_swapchain.GetAddressOf() << std::endl;
    log();

    if (!g_windowed)
    {
        HRESULT hr = g_refresh_swapchain->SetFullscreenState(TRUE, nullptr);
        if (SUCCEEDED(hr))
        {
            hr = g_refresh_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            g_out << "   exclusive fullscreen engaged (ResizeBuffers: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        }
        else
        {
            g_out << "   SetFullscreenState(TRUE) failed: 0x" << std::hex << hr << std::dec << ", staying windowed" << std::endl;
        }
        log();
    }

    // Backbuffer fetched once (after any fullscreen transition) and reused
    // across every Present - validated for this flip-model chain.
    HR(g_refresh_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(refresh_backbuffer.GetAddressOf())));

    // Open the handoff ring on the presenter device, and create the local
    // 2-slice pair: both presents of a pair always come from the same snapshot.
    for (int i = 0; i < HANDOFF_SLOTS; i++)
    {
        HR(refresh_device->OpenSharedResource(g_handoff_handle[i], __uuidof(ID3D11Texture2D), (void**)&handoff_share[i]));
        HR(handoff_share[i].As(&handoff_mutex[i]));
    }
    D3D11_TEXTURE2D_DESC pair_desc;
    handoff_share[0]->GetDesc(&pair_desc);
    pair_desc.MiscFlags = 0;
    pair_desc.BindFlags = 0;
    HR(refresh_device->CreateTexture2D(&pair_desc, nullptr, &local_pair));

    // Presenter side of the handoff (geo-11's AdoptPair): drain the ring in
    // order; the readiness fence gate (F9) refuses a pair whose GPU copy is
    // still queued behind the game's frame - queuing our copy then would make
    // this thread's whole queue (pair copy, backbuffer copy, Present) inherit
    // that wait, drain the flip queue, and repeat one eye on screen: the
    // sub-60 wobble under test. Sub-timed for the slip diagnostics.
    UINT   read_index          = 0;
    UINT64 last_adopted_fence  = 0;
    float  adopt_acquire_ms = 0, adopt_copy_ms = 0, adopt_release_ms = 0;

    LARGE_INTEGER qpc_freq;
    QueryPerformanceFrequency(&qpc_freq);
    const double qpc_to_ms = 1000.0 / (double)qpc_freq.QuadPart;

    auto adopt_pair = [&](DWORD timeout_ms) -> bool {
        int           slot = read_index % HANDOFF_SLOTS;
        LARGE_INTEGER t0, t1, t2, t3;
        QueryPerformanceCounter(&t0);

        // Readiness gate, checked BEFORE AcquireSync ever runs: the acquire
        // is not trustworthy as a non-blocking probe - on an 11on12 device it
        // has been measured blocking 5-10ms even with a 0 timeout, waiting on
        // the game's still-executing handoff copy. That is the presenter
        // waiting on the game, the exact thing this architecture forbids
        // (suspected root of the geo-11 sub-120fps wobble). FIFO fence values
        // are sequential - one per delivered pair - so the next pair's value
        // is known (last adopted + 1) without reading anything the game owns:
        // this check is pure CPU, and once it passes, the pair's GPU copy is
        // complete AND its ReleaseSync (issued just after the Signal) has
        // happened, so the acquire below succeeds instantly.
        if (g_fence_gate && g_handoff_fence && g_handoff_fence->GetCompletedValue() < last_adopted_fence + 1)
        {
            adopt_acquire_ms = 0;
            adopt_copy_ms    = 0;
            adopt_release_ms = 0;
            return false;  // Pair not GPU-complete yet: reuse the current pair, never wait.
        }

        HRESULT hr       = handoff_mutex[slot]->AcquireSync(1, timeout_ms);
        QueryPerformanceCounter(&t1);
        adopt_acquire_ms = (float)((t1.QuadPart - t0.QuadPart) * qpc_to_ms);
        adopt_copy_ms    = 0;
        adopt_release_ms = 0;
        if (hr != S_OK)  // WAIT_TIMEOUT (no new pair) or failure: reuse the current pair.
            return false;

        // Post-acquire backstop (should never fire once the pre-check above
        // passed - the fence is monotonic - but cheap defense in depth).
        if (g_fence_gate && g_handoff_fence && g_handoff_fence->GetCompletedValue() < g_slot_fence_value[slot])
        {
            handoff_mutex[slot]->ReleaseSync(1);  // Hand it back untouched; retry next left vblank.
            return false;
        }

        refresh_context->CopyResource(local_pair.Get(), handoff_share[slot].Get());
        QueryPerformanceCounter(&t2);
        adopt_copy_ms = (float)((t2.QuadPart - t1.QuadPart) * qpc_to_ms);

        handoff_mutex[slot]->ReleaseSync(0);
        QueryPerformanceCounter(&t3);
        adopt_release_ms = (float)((t3.QuadPart - t2.QuadPart) * qpc_to_ms);

        last_adopted_fence = g_slot_fence_value[slot];
        read_index++;
        return true;
    };

    // Take an initial copy of the eye pair, so the first presents show real
    // data. Retry loop, because adopt_pair also refuses fence-incomplete pairs.
    {
        ULONGLONG seed_start = GetTickCount64();
        bool      seeded     = false;
        while (g_running && !(seeded = adopt_pair(2)) && GetTickCount64() - seed_start < 2000)
            Sleep(1);
        if (!seeded)
        {
            g_out << "** initial pair seed timed out" << std::endl;
            log();
        }
    }

    // Emitter metronome: the glasses are commanded from a dedicated vblank-paced
    // thread, never from Present time. The emitter free-runs its own shutter
    // timer and treats AA commands as a phase resync, without reliably honoring
    // the eye identity byte- so the command stream must be a steady, strictly
    // alternating cadence. Present-time commands jitter exactly when the
    // presenter is recovering from a stall, and one badly timed burst re-phases
    // the emitter into a persistent eye swap even though the on-screen images
    // are correct. This thread also keeps commands flowing while the presenter
    // is occluded, so the glasses neither drift nor hit the emitter's ~4s idle
    // shutoff during alt-tab.
    ComPtr<IDXGIOutput> refresh_output;
    HR(g_refresh_swapchain->GetContainingOutput(&refresh_output));

    std::thread emitter_thread([refresh_output]() 
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        while (g_running)
        {
            if (FAILED(refresh_output->WaitForVBlank()))
            {
                DebugBreak();
                Sleep(8);
                continue;
            }

            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);

            VBlankAnchor anchor;
            {
                std::lock_guard<std::mutex> hold(g_vblank_anchor_mutex);
                anchor = g_vblank_anchor;
            }
            if (!anchor.valid)
                continue;  // Glasses coast on the emitter's internal timer until stats arrive.

            // Index of the refresh that just began scanning out. Deriving it from
            // the QPC anchor instead of counting wakeups keeps the parity exact
            // even when this thread misses vblanks.
            UINT refresh = anchor.sync_refresh + (UINT)((now.QuadPart - anchor.sync_qpc) / anchor.period_qpc + 0.5);
            bool left    = (refresh & 1) == 0;

            // g_eye_swap inverts the command relative to the image: the calibration
            // for chains where the protocol's 'left' actually opens the right lens,
            // or the content's L/R is reversed.
            if (left != (g_eye_swap != 0))
                g_shutterGlasses.SetLeftEye();
            else
                g_shutterGlasses.SetRightEye();
        }
    });

    // Parity-driven presenter.
    //
    // Eye identity is derived from the absolute refresh count of the vblank each
    // Present lands on (even = left, odd = right), never
    // from alternation. GetFrameStatistics is the ground truth for which refresh
    // each present actually appeared on; the prediction is re-anchored from it
    // every frame, so any stall degrades to a repeated eye for one frame instead
    // of a persistent eye-swap.

    struct present_record
    {
        UINT  present_count;
        UINT  predicted_refresh;
        bool  left;
        bool  anchored;          // Predicted with a valid stats anchor.
        float gap_ms;            // Since the previous Present returned (stats readback + loop overhead).
        float adopt_ms;          // AcquireSync + fence gate + pair CopyResource submit.
        float copy_ms;           // Backbuffer copy submit.
        float present_ms;        // The Present call including its pacing block.
        float adopt_acquire_ms;  // AcquireSync / CopyResource / ReleaseSync
        float adopt_copy_ms;     // breakdown of adopt_ms; all zero on
        float adopt_release_ms;  // right-presents (no adoption attempted).
    };
    present_record history[16] = {};

    bool     stats_valid           = false;  // Anchor usable? False until first stats arrive, and after disjoint/occlusion.
    UINT     anchor_present        = 0;      // stats.PresentCount at the anchor.
    UINT     anchor_refresh        = 0;      // stats.PresentRefreshCount at the anchor.
    UINT     presents_issued       = 0;      // GetLastPresentCount after our latest Present.
    UINT     last_verified_present = 0;
    bool     prev_left             = false;
    int      startup_log           = 8;
    LONGLONG prev_present_return   = 0;
    bool     occluded              = false;

    UINT     period_base_refresh = 0;  // First sync record since the last disjoint;
    LONGLONG period_base_qpc     = 0;  // the vblank period is measured from here.

    while (g_running)
    {
        // F6: injected stall, simulating this thread being preempted by the
        // scheduler- the original eye-swap vector. Recovery should be a slip
        // report, a repeated eye, and re-anchored parity; never a swap.
        int stall_ms = g_stall_request_ms.exchange(0);
        if (stall_ms)
        {
            g_out << "** F6: injected presenter stall of " << stall_ms << " ms" << std::endl;
            log();
            Sleep(stall_ms);
        }

        // Predict the refresh this Present will appear on. With frame latency 1,
        // each present not yet reported by the stats occupies one refresh after
        // the last reported one.
        UINT predicted_refresh;
        if (stats_valid)
            predicted_refresh = anchor_refresh + (presents_issued - anchor_present) + 1;
        else
            predicted_refresh = presents_issued + 1;  // Free-running until stats arrive.

        bool left = (predicted_refresh & 1) == 0;

        if (stats_valid && left == prev_left && startup_log <= 0)
        {
            g_out << "== Repeating " << (left ? "L" : "R") << " eye to restore parity." << std::endl;
            log();
        }
        prev_left = left;

        // Pair boundary: only adopt a new game pair when the upcoming vblank is a
        // left, so a pair can never be split across a boundary (no L/R images from
        // different game frames). Try-acquire (0 timeout): if the game hasn't
        // produced a new (GPU-complete) pair, adopt_pair returns false and the
        // previous pair is reused - the presenter never blocks on the game.
        LARGE_INTEGER t_start, t_adopt, t_copy, t_present;
        QueryPerformanceCounter(&t_start);

        adopt_acquire_ms = 0;
        adopt_copy_ms    = 0;
        adopt_release_ms = 0;
        if (left)
            adopt_pair(0);
        QueryPerformanceCounter(&t_adopt);

        refresh_context->CopySubresourceRegion(refresh_backbuffer.Get(), 0, 0, 0, 0, local_pair.Get(), left ? 0 : 1, nullptr);
        QueryPerformanceCounter(&t_copy);

        HRESULT hr = g_refresh_swapchain->Present(1, 0);
        QueryPerformanceCounter(&t_present);

        // Teardown: the main thread drops g_running, then releases exclusive
        // fullscreen while this thread may be mid-loop. Swapchain calls fail
        // during that transition - exit before treating any of them as fatal.
        if (!g_running)
            break;

        if (hr == DXGI_STATUS_OCCLUDED)
        {
            // Alt-tab etc: nothing reached the screen and the stats go stale.
            // Idle, and re-baseline parity when presents start landing again.
            stats_valid         = false;
            prev_present_return = 0;
            if (!occluded)
            {
                occluded = true;
                g_out << "** Presenter occluded. Parity re-baseline pending." << std::endl;
                log();
            }

            // Keep draining the handoff while occluded, regardless of parity:
            // nothing is displayed so pair-boundary discipline doesn't apply,
            // but the FIFO game would otherwise block on a full ring whenever
            // the frozen parity stopped on a right.
            adopt_pair(0);

            Sleep(5);
            continue;
        }
        occluded = false;
        HR(hr);

        HR(g_refresh_swapchain->GetLastPresentCount(&presents_issued));
        history[presents_issued % ARRAYSIZE(history)] = { presents_issued, predicted_refresh, left, stats_valid,
            prev_present_return ? (float)((t_start.QuadPart - prev_present_return) * qpc_to_ms) : 0.0f,
            (float)((t_adopt.QuadPart - t_start.QuadPart) * qpc_to_ms),
            (float)((t_copy.QuadPart - t_adopt.QuadPart) * qpc_to_ms),
            (float)((t_present.QuadPart - t_copy.QuadPart) * qpc_to_ms),
            adopt_acquire_ms, adopt_copy_ms, adopt_release_ms };
        prev_present_return = t_present.QuadPart;

        if (startup_log > 0)
        {
            g_out << "   present " << presents_issued << " (" << (left ? "L" : "R") << ")"
                  << " predicted refresh: " << predicted_refresh
                  << (stats_valid ? "" : "  [no anchor]") << std::endl;
            log();
            startup_log--;
        }

        // Verify against ground truth, and re-anchor the prediction.
        DXGI_FRAME_STATISTICS stats = {};
        hr                          = g_refresh_swapchain->GetFrameStatistics(&stats);
        if (hr == DXGI_ERROR_FRAME_STATISTICS_DISJOINT)
        {
            stats_valid = false;

            // The refresh counter or display mode may have changed, so the vblank
            // clock is no longer trustworthy. The metronome goes quiet until a
            // fresh anchor is published.
            period_base_refresh = 0;
            {
                std::lock_guard<std::mutex> hold(g_vblank_anchor_mutex);
                g_vblank_anchor.valid = false;
            }
            g_out << "** Frame statistics disjoint. Parity re-baseline pending." << std::endl;
            log();
        }
        else if (SUCCEEDED(hr) && stats.SyncQPCTime.QuadPart != 0)
        {
            // Publish the vblank clock for the emitter metronome. The period is
            // measured over the whole span since the last disjoint, so it converges
            // on the true refresh period and stays accurate when extrapolated
            // across long occlusions.
            if (period_base_refresh == 0)
            {
                period_base_refresh = stats.SyncRefreshCount;
                period_base_qpc     = stats.SyncQPCTime.QuadPart;
            }
            else if (stats.SyncRefreshCount > period_base_refresh)
            {
                std::lock_guard<std::mutex> hold(g_vblank_anchor_mutex);
                g_vblank_anchor.sync_qpc     = stats.SyncQPCTime.QuadPart;
                g_vblank_anchor.sync_refresh = stats.SyncRefreshCount;
                g_vblank_anchor.period_qpc   = double(stats.SyncQPCTime.QuadPart - period_base_qpc) / (stats.SyncRefreshCount - period_base_refresh);
                g_vblank_anchor.valid        = true;
            }

            if (stats.PresentCount == 0 || stats.PresentCount == last_verified_present)
                continue;
            last_verified_present = stats.PresentCount;

            const present_record& rec = history[stats.PresentCount % ARRAYSIZE(history)];
            if (rec.present_count == stats.PresentCount)
            {
                if (stats_valid && rec.anchored && stats.PresentRefreshCount != rec.predicted_refresh)
                {
                    g_out << "!! present " << rec.present_count << " (" << (rec.left ? "L" : "R") << ")"
                          << " predicted refresh: " << rec.predicted_refresh
                          << " landed on: " << stats.PresentRefreshCount
                          << "  slip: " << (int)(stats.PresentRefreshCount - rec.predicted_refresh) << std::endl;

                    // Which leg of the loop ate the frame? This present's
                    // timings and the preceding one's (the miss is often the
                    // prior iteration overrunning).
                    const present_record& prev = history[(stats.PresentCount - 1) % ARRAYSIZE(history)];
                    g_out << "    timings(ms) this: gap=" << rec.gap_ms << " adopt=" << rec.adopt_ms
                          << " (acq=" << rec.adopt_acquire_ms << " copy=" << rec.adopt_copy_ms << " rel=" << rec.adopt_release_ms << ")"
                          << " copy=" << rec.copy_ms << " present=" << rec.present_ms << std::endl;
                    if (prev.present_count == stats.PresentCount - 1)
                        g_out << "    timings(ms) prev: gap=" << prev.gap_ms << " adopt=" << prev.adopt_ms
                              << " (acq=" << prev.adopt_acquire_ms << " copy=" << prev.adopt_copy_ms << " rel=" << prev.adopt_release_ms << ")"
                              << " copy=" << prev.copy_ms << " present=" << prev.present_ms << std::endl;
                    log();
                }
                if (!stats_valid)
                {
                    g_out << "== Parity anchor: present " << stats.PresentCount
                          << " on refresh " << stats.PresentRefreshCount
                          << "  QPC: " << stats.SyncQPCTime.QuadPart << std::endl;
                    log();
                }
                anchor_present = stats.PresentCount;
                anchor_refresh = stats.PresentRefreshCount;
                stats_valid    = true;
            }
        }
    }
    g_out << " refresh thread loop exit-> " << std::endl;
    log();

    emitter_thread.join();

    local_pair.Reset();
    for (int i = 0; i < HANDOFF_SLOTS; i++)
    {
        handoff_mutex[i].Reset();
        handoff_share[i].Reset();
    }

    // Not legal to do from this thread. Hangs.
    // SetFullscreenState(false) is issued by the main thread (restart_presenter)
    // BEFORE this thread is joined - the validated ordering.
    log();

    // All components are ComPtr and will automatically be disposed.
    g_out << " <-- refresh_thread Exit" << std::endl;
    log();
}
