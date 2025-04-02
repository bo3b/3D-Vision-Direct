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
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11.h>
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

#include "nvapi.h"

#include "Timer.h"
#include "ShutterGlasses.h"
#include "resource.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT          init_window(HINSTANCE hInstance, int nCmdShow);
void             start_glasses();
void             enable_lightboost();
HRESULT          init_dx11();
void             cleanup_device();
LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
void             render_frame();
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
HINSTANCE g_hInst      = nullptr;
HWND      g_hWnd       = nullptr;
bool      g_fullScreen = false;

ID3D11Device*        g_pd3dDevice        = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain*      g_pSwapChain        = nullptr;

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

ComPtr<ID3D11Texture2D>        g_right_eye_tex;
ComPtr<ID3D11Texture2D>        g_left_eye_tex;
ComPtr<ID3D11RenderTargetView> g_right_eye_RTV;
ComPtr<ID3D11RenderTargetView> g_left_eye_RTV;
HANDLE                         g_right_eye_handle;
HANDLE                         g_left_eye_handle;
std::mutex                     g_drawing_mutex;

//--------------------------------------------------------------------------------------
// Frank Luna style error checking for stuff that should never fail.
//--------------------------------------------------------------------------------------
void HR(
    HRESULT hresult)
{
    if (FAILED(hresult))
    {
        std::ostringstream error_log;

        error_log << __FILE__ << ", " << __LINE__ << ", HR: " << hresult << std::endl;
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

    if (FAILED(init_window(hInstance, nCmdShow)))
        return 0;

    if (FAILED(init_dx11()))
    {
        cleanup_device();
        return 0;
    }

    // Start a rendering subthread, so that rendering is off the main app thread,
    // and thus UI things like dragging the window don't block drawing.
    g_running      = true;
    g_renderThread = std::thread(refresh_thread);

    // Set thread priority to highest, to make sure it is not stalled by
    // normal processes. Doesn't stop eye-flips, but is conceptually right.
    // Could maybe justify THREAD_PRIORITY_TIME_CRITICAL real time.
    HANDLE hThread = g_renderThread.native_handle();
    if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST))
    {
        g_out << "Failed to set thread priority: " << GetLastError() << std::endl;
        log();
    }

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
            if (GetAsyncKeyState(VK_F2) & 0x8000)
            {
                g_shutterGlasses.ToggleEyes();
            }
            if (GetAsyncKeyState(VK_F4) & 0x8000)
            {
                try
                {
                    g_running = false;
                    if (g_renderThread.joinable())
                    {
                        g_renderThread.join();  // Wait for it to cleanly exit.
                    }

                    g_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);

                    g_out << ">> SetFullScreenState" << std::endl;
                    log();

                    g_fullScreen = !g_fullScreen;
                    HRESULT hr   = g_pSwapChain->SetFullscreenState(g_fullScreen, nullptr);

                    Sleep(100);
                    g_out << "<< SetFullScreenState" << std::endl;
                    log();

                    if (FAILED(hr))
                        DebugBreak();
                    hr = g_pSwapChain->ResizeBuffers(g_bufferCount, g_ScreenWidth, g_ScreenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
                    if (FAILED(hr))
                        DebugBreak();

                    Sleep(200);

                    BOOL isFullscreen = FALSE;
                    g_pSwapChain->GetFullscreenState(&isFullscreen, nullptr);
                    DXGI_SWAP_CHAIN_DESC desc;
                    g_pSwapChain->GetDesc(&desc);
                    g_out << "Post SetFullscreenState is Fullscreen: " << isFullscreen << std::endl;
                    g_out << "Post SetFullscreenState Flags: 0x" << std::hex << desc.Flags << std::dec << std::endl;
                    log();

                    g_pSwapChain->Present(1, DXGI_PRESENT_RESTART);

                    IDXGIFactory5* pFactory = nullptr;
                    g_pSwapChain->GetParent(__uuidof(IDXGIFactory5), reinterpret_cast<void**>(&pFactory));

                    BOOL allowTearing = FALSE;
                    hr                = pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
                    if (FAILED(hr))
                        DebugBreak();
                    g_out << "Tearing support: " << allowTearing << std::endl;
                    log();
                    if (pFactory)
                        pFactory->Release();

                    g_running      = true;
                    g_renderThread = std::thread(refresh_thread);  // Restart drawing
                }
                catch (...)
                {
                    DebugBreak();
                }
            }
        }
        else
        {
            render_frame();  // both eyes
        }
    }

    // Cleanly stop drawing thread upon exit
    g_running = false;
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
HRESULT init_window(HINSTANCE hInstance, int nCmdShow)
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

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, g_ScreenWidth, g_ScreenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 7", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);

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
    desc.OutputWindow                       = g_hWnd;
    desc.SampleDesc.Count                   = 1;
    desc.SampleDesc.Quality                 = 0;
    desc.Windowed                           = TRUE;
    desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

    // Create the simple DX11, Device, SwapChain, and Context.
    HR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_device_flags, nullptr, 0, D3D11_SDK_VERSION, &desc, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext));

    // Create the offscreen Texture2D for each eye that we will DrawIndexed into.
    // These are SharedSurfaces so that they can be used for Present in the Refresh Thread.
    // They need to be identical to the drawing backbuffer in size, color format.
    // We create them as Shared so that the refresh thread can access latest images.

    ComPtr<ID3D11Texture2D> drawing_backbuffer;
    HR(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(drawing_backbuffer.GetAddressOf())));

    D3D11_TEXTURE2D_DESC texture_desc;
    drawing_backbuffer->GetDesc(&texture_desc);
    texture_desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;

    HR(g_pd3dDevice->CreateTexture2D(&texture_desc, nullptr, &g_right_eye_tex));
    HR(g_pd3dDevice->CreateTexture2D(&texture_desc, nullptr, &g_left_eye_tex));

    HR(g_pd3dDevice->CreateRenderTargetView(g_right_eye_tex.Get(), nullptr, &g_right_eye_RTV));
    HR(g_pd3dDevice->CreateRenderTargetView(g_left_eye_tex.Get(), nullptr, &g_left_eye_RTV));

    ComPtr<IDXGIResource> right_eye_dxgi;
    HR(g_right_eye_tex.As(&right_eye_dxgi));
    HR(right_eye_dxgi->GetSharedHandle(&g_right_eye_handle));
    ComPtr<IDXGIResource> left_eye_dxgi;
    HR(g_left_eye_tex.As(&left_eye_dxgi));
    HR(left_eye_dxgi->GetSharedHandle(&g_left_eye_handle));

    // For DX11 3D, it's required that we run in exclusive full-screen mode, otherwise 3D
    // Vision will not activate.
    //hr = g_pSwapChain->SetFullscreenState(TRUE, nullptr);
    //if (FAILED(hr))
    //    return hr;

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

                if (g_pSwapChain)
                {
                    // Resize the swapchain buffers
                    HRESULT hr = g_pSwapChain->ResizeBuffers(g_bufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
                    if (FAILED(hr))
                        DebugBreak();
                }
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
// Render current image, eye independent.
//--------------------------------------------------------------------------------------
void draw_cube(bool rightEye)
{
    //
    // Clear the buffer
    //
    if (rightEye)
        g_pImmediateContext->ClearRenderTargetView(g_right_eye_RTV.Get(), Colors::MidnightBlue);
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
    // Projection matrix in g_pSharedCB determines eye view.
    //
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pSharedCB);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->DrawIndexed(36, 0, 0);
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
// Render a frame, both eyes. But do not Present.
//--------------------------------------------------------------------------------------
void render_frame()
{
    //
    // Rotate cube around the origin
    //
    g_World = XMMatrixRotationY(GetTickCount64() / 1000.0f);

    //
    // This now includes changing CBChangeOnResize each frame as well, because
    // we need to update the Projection matrix each frame, in case the user changes
    // the 3D settings.
    // The variable names are a bit misleading at present.
    //
    shared_CB cb                    = {};
    float     eye_convergence       = 4.0f;
    float     eye_separation        = 10.10f;
    float     separation_percentage = 0.52f;

    float separation  = eye_separation * separation_percentage / 100;
    float convergence = eye_separation * separation_percentage / 100 * eye_convergence;

    //stall += 10;
    //sleep_microseconds(stall);

    // Checking for possible fatal errors that could cause an eye swap situation.
    // Does not seem to ever hit exception handler, which is what we'd expect.
    try
    {
        // We want to lock around the drawing, so that the refresh can not get
        // half baked results.
        g_drawing_mutex.lock();
        {
            // <----------------------- Left Eye -------------------------------
            //
            // Specifically set the LeftEye as active, not just toggle. This seems
            // to help get proper sync when the app is active, but doesn't help with
            // alt-tab eye swaps.
            double left_eye_start = g_Timer.GetElapsedMicroseconds();

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
                g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

                draw_cube(false);
            }
            //g_pImmediateContext->Flush();
            //hr = g_pSwapChain->Present(1, DXGI_PRESENT_TEST);
            //if (FAILED(hr))
            //{
            //    HRESULT reason = g_pd3dDevice->GetDeviceRemovedReason();
            //    g_out << "Present failed: " << hr << "  Reason: " << reason << std::endl;
            //    log();
            //    DebugBreak();
            //}

            double left_eye_elapsed = (g_Timer.GetElapsedMicroseconds() - left_eye_start) / 1000.0f;
            if (left_eye_elapsed > 18.0f)
            {
                g_out << "!! Left frame dropped. Eye swap." << std::endl;
                log();
                out_limit = 2;
                //g_shutterGlasses.InitEmitter();	// re-init on drops
            }
            if (out_limit > 0)
            {
                g_out << "Left eye frame time:  " << left_eye_elapsed << " ms" << std::endl;
                log();
            }

            // <----------------------- Right Eye -------------------------------
            //
            double right_eye_start = g_Timer.GetElapsedMicroseconds();

            {
                cb.mWorld = XMMatrixTranspose(g_World);
                cb.mView  = XMMatrixTranspose(g_View);

                cb.mProjection = g_Projection;
                cb.mProjection._31 += separation;
                cb.mProjection._41 = -convergence;
                cb.mProjection     = XMMatrixTranspose(cb.mProjection);
                g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

                draw_cube(true);
            }
            //g_pImmediateContext->Flush();
            //hr = g_pSwapChain->Present(1, DXGI_PRESENT_TEST);
            //if (FAILED(hr))
            //{
            //    HRESULT reason = g_pd3dDevice->GetDeviceRemovedReason();
            //    g_out << "Present failed: " << hr << "  Reason: " << reason << std::endl;
            //    log();
            //    DebugBreak();
            //}

            double right_eye_elapsed = (g_Timer.GetElapsedMicroseconds() - right_eye_start) / 1000.0f;
            if (right_eye_elapsed > 18.0f)
            {
                g_out << "!! Right frame dropped. Eye swap." << std::endl;
                log();
                out_limit = 2;
                //g_shutterGlasses.InitEmitter();	// re-init on drops
            }
            if (out_limit > 0)
            {
                g_out << "Right eye frame time: " << right_eye_elapsed << " ms" << std::endl;
                log();
            }

            double current_frame_time = g_Timer.GetElapsedMicroseconds();
            if (out_limit > 0)
            {
                g_out << "  full frame time:             " << (current_frame_time - g_lastFrame) / 1000.0f << " ms" << std::endl;
                log();

                out_limit--;
            }
            g_lastFrame = current_frame_time;
        }
        g_drawing_mutex.unlock();
        g_pImmediateContext->Flush();

        // Stall around to slower than refresh rate- for testing.
        Sleep(1000 / 20);
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
    ComPtr<IDXGISwapChain>      refresh_swapchain;
    ComPtr<ID3D11Device>        refresh_device;
    ComPtr<ID3D11DeviceContext> refresh_context;
    ComPtr<ID3D11Texture2D>     refresh_backbuffer;

    ComPtr<ID3D11Texture2D> right_eye_share;
    ComPtr<ID3D11Texture2D> left_eye_share;
    ComPtr<ID3D11Texture2D> refresh_left_eye;
    ComPtr<ID3D11Texture2D> refresh_right_eye;

    // Upon startup, we need to create our output SwapChain that is a copy of the main
    // drawing environment.  It is going to draw directly to the main window. We duplicate
    // the Description and Device Flags so as to be exactly the same output, which will
    // allow us to use CopyResource.
    // We tweak the BufferCount and SwapEffect to avoid conflicts with whatever the
    // game specified for them.

    DXGI_SWAP_CHAIN_DESC desc = {};
    g_pSwapChain->GetDesc(&desc);
    desc.BufferCount  = 1;
    desc.SwapEffect   = DXGI_SWAP_EFFECT_SEQUENTIAL;
    UINT device_flags = g_pd3dDevice->GetCreationFlags();

    HR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, nullptr, 0, D3D11_SDK_VERSION, &desc, &refresh_swapchain, &refresh_device, nullptr, &refresh_context));

    HR(refresh_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(refresh_backbuffer.GetAddressOf())));

    // The original shared surfaces, updated by the game render loop.
    HR(refresh_device->OpenSharedResource(g_right_eye_handle, __uuidof(ID3D11Texture2D), (void**)&right_eye_share));
    HR(refresh_device->OpenSharedResource(g_left_eye_handle, __uuidof(ID3D11Texture2D), (void**)&left_eye_share));

    // A local copy of the game data, so that it can independently display with no sync requirement.
    D3D11_TEXTURE2D_DESC eye_desc;
    right_eye_share.Get()->GetDesc(&eye_desc);
    HR(refresh_device->CreateTexture2D(&eye_desc, nullptr, &refresh_right_eye));
    left_eye_share.Get()->GetDesc(&eye_desc);
    HR(refresh_device->CreateTexture2D(&eye_desc, nullptr, &refresh_left_eye));

    while (g_running)
    {
        // Fetch the local copy of left eye data, and copy to backbuffer. The Present(1,0) will wait
        // until next vblank to show it.

        refresh_context->CopyResource(refresh_backbuffer.Get(), refresh_left_eye.Get());
        g_shutterGlasses.SetLeftEye();
        HR(refresh_swapchain->Present(1, 0));

        // Next frame in frame-sequential output will be right eye. Waits for the vblank.

        refresh_context->CopyResource(refresh_backbuffer.Get(), refresh_right_eye.Get());
        g_shutterGlasses.SetRightEye();
        HR(refresh_swapchain->Present(1, 0));

        // Right after we have finished the update for both eyes, we'll have a full frame
        // time to catch up with new eye data. We'll wait here by mutex for any drawing
        // to complete, then make a local copy to use for next 2 frames.

        g_drawing_mutex.lock();
        {
            refresh_context->CopyResource(refresh_left_eye.Get(), left_eye_share.Get());
            refresh_context->CopyResource(refresh_right_eye.Get(), right_eye_share.Get());
        }
        g_drawing_mutex.unlock();
    }
}
