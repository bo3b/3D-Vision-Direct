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

#include "Utils.h"
#include "Timer.h"
#include "Params.h"

#include "StereoRender.h"
#include "resource.h"

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT          init_windows(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE g_hInst       = nullptr;
HWND      g_hWnd        = nullptr;
HWND      g_hidden_hWnd = nullptr;
bool      g_windowed    = true;

Timer  g_Timer;
double g_lastFrame = 0;

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(init_windows(hInstance, nCmdShow)))
        return 0;

    if (FAILED(init_dx11(g_hWnd)))
    {
        cleanup_device();
        return 0;
    }

    // Main message and drawing loop
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
            //{
            //    static bool f2_was_down = false;
            //    bool        f2_down     = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
            //    if (f2_down && !f2_was_down)
            //        g_eye_swap ^= 1;
            //    f2_was_down = f2_down;
            //}

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
                }
                f4_was_down = f4_down;
            }
        }
        else
        {
            render_frame();  // both eyes
        }
    }

    // On escape for exit clean up and dispose objects.
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
    RECT rc = { 0, 0, (LONG)g_ScreenWidth, (LONG)g_ScreenHeight };
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

    //g_hidden_hWnd = CreateWindow(L"TutorialWindowClass", L"Hidden", WS_BORDER, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    //if (!g_hidden_hWnd)
    //    return E_FAIL;
    //g_out << "Secondary hidden render window created: " << g_hidden_hWnd << std::endl;
    //log();

    g_Timer.Start();
    g_out << std::fixed << std::setprecision(2);

    return S_OK;
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
            }
            break;

            // Note that this tutorial does not handle resizing (WM_SIZE) requests,
            // so we created the window without the resize border.

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
