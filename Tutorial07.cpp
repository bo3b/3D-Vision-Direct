//--------------------------------------------------------------------------------------
// File: Tutorial07.cpp
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
//--------------------------------------------------------------------------------------

#include "resource.h"

#include <d3d11.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <directxcolors.h>
#include <directxmath.h>
#include <windows.h>

#include "nvapi.h"
#include "nvapi_lite_stereo.h"

#include <exception>
using namespace DirectX;

//--------------------------------------------------------------------------------------
// More notes about test results.
//
// 1) It is possible to have both DX9 and DX11 devices target the same window.
// This does not give any errors if both are set to DISCARD. However, they
// will interfere with each other in windowed mode, causing flickering between
// the competing images. In fullscreen, DX11 takes over and is only one visible,
// even though the DX9 comes later.
// If they are set to FLIP, then only DX11 will be seen, it seems to preempt
// the DX9 Present no matter the settings.
// It's not possible to use FLIPEX, as that returns an error if both use it,
// and also 3D does not engage when using FLIPEX on DX9 path.
// It makes the most sense to use DX9 FLIP for windowed, and DISCARD for both
// when fullscreen.
// There does not seem to be any combination for parameters that will allow the
// DX9 present to take precedence over the dx11 one, when both are in the same
// window. Even setting the dx11_sync_interval to 4, and dx9_flags to IMMEDIDATE
// did not allow dx9 to be seen.
// 2) Setting the DX11 to use a different window works.
// This immediately works for windowed mode, because the extra window for the
// dx11 side is invisible to start, and left invisible.  So the dx11 Present
// doesn't draw anything.
// This works in both windowed and fullscreen, even when the DX11 side is not
// resized.  DX9 side is done via ResetEx.  The dx11 side not being resized
// still works because the window it targets is not frontmost.
// Making a WS_CHILD window is probably more compatible, because then it
// cannot interfere with making the main window frontmost.  Here during testing
// both window types work the same and have no negative effects.
// 3) FLIPEX mode for DX9 works, but only in fullscreen mode.
// No errors reported as long as the dx11 is windowed, so dx9 FLIPEX and
// dx11 FLIP_DISCARD is successful in fullscreen.  It's worth noting that for
// whatever reason that even while it is shuttering for other modes, the only
// mode we see the nvidia overlay is in FLIPEX for dx9.  When dx11 goes to
// same window, it fails with Access_Denied.  If dx11 is DISCARD instead, it
// does not crash, but 3D does not engage.  This works without a profile for
// the test app, and nvidia default overlay comes up. So FLIPEX disables the
// stereo profile bit handling.  Using known good GoogleEarth profile, we
// do not get 3D in windowed mode using FLIPEX.  A quote from 3DFM notes:
// "New: For drivers beyond 452.06 stereo 3D screenshots are forced into
// fullscreen mode as windowed mode no longer works for these drivers."
// Since the goal of this is support on latest drivers, that means fullscreen
// only should be the target and thus FLIPEX is default.
// dx11 present before or after dx9 present works. dx11 Present first is
// slightly better, because overlays will then draw and be sent stereo.

D3DSWAPEFFECT    dx9_swap_mode      = D3DSWAPEFFECT::D3DSWAPEFFECT_DISCARD;
DXGI_SWAP_EFFECT dx11_swap_mode     = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
UINT             dx11_sync_interval = 1;
UINT             dx11_flags         = 0;
DWORD            dx9_flags          = D3DPRESENT_INTERVAL_ONE;
bool             dx11_present_first = true;
bool             dx11_window        = true;
bool             dx11_make_child    = true;
bool             dx11_skip          = false;
UINT             g_ScreenWidth      = 1920;
UINT             g_ScreenHeight     = 1080;

// Some notes:
//  It would be interesting to use the D3DSWAPEFFECT_FLIPEX SwapEffect, as that can then
//  allow for D3DPRESENT_FORCEIMMEDIATE to be used at the DX9->Present call.  That would
//  in theory preempt a pending DX11 frame, and show only the DX9 stereo output. However-
//  this fails with an Access_Denied error.  It is apparently not legal to have more than
//  one device using this SwapEffect mode for a given window.  It fails whether DX9 is init
//  first or last.  It fails if I do two dx9 in a row.
//
//  We must use d3d9Ex in order to allow for surface sharing, so the Device is Device9Ex.
//  The Device9Ex is created with the primary window as the output, so that the swap chain
//  used will be the main output, not the child window for dx11.
//
//  Creating a child window of the primary window seems to work as desired.  The swapchain
//  for the DX11 device is targeting the child window, which is created right before DX11
//  init, and is set to not-visible.  Then, DX9 setup is done, which targets the primary
//  window as the swapchain output, and this then shutters properly, showing L/R colors
//  that are different for each eye, as set via the SetActiveEye.  Next step would be to
//  share surface the DX11 output to the DX9 side, but I think it's not worth the effort,
//  as it's clearly working.  Whenever DX11 was primary output, even accidentally, it would
//  show spinning cube on blue background, and nothing showing for DX9 colors.
//  This will also keep games working, because the child window is invisible to all other
//  callers, and so any events should still go to the game-owned main window.
//  If the child window is set to Shown, then the DX11 present takes over and blocks the
//  DX9 present, even as 3D Vision is actively shuttering.
//
//  With different Present flags, the frame might be busy, and then the BeginScene would
//  fail with a busy-GPU error.  So the output from the DX11 needs to be buffered, so that
//  the DX9 side can preempt it.  We don't want to change any flip mode behavior for games,
//  so this should work.
//
//  This also works in windowed mode, which should be quite a lot better for future games
//  and drivers, because Windows is moving away from exclusive full screen in all cases.
//  This should work correctly in a borderless full screen mode.
//
//  Need to support exclusive full screen too, Drivers 5xx don't seem to run DX9 windowed
//  anymore.  So for this, we'll follow the UE4 approach, where it starts windowed, then
//  switches full screen.  That'll be done in ActivateStereo to reset both swapchains.

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
};

struct SharedCB
{
    XMMATRIX mWorld;
    XMMATRIX mView;
    XMMATRIX mProjection;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE g_hInst      = NULL;
HWND      g_hWnd       = NULL;
HWND      g_child_hWnd = NULL;

ID3D11Device*        g_pd3dDevice        = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain*      g_pSwapChain        = nullptr;

// One for each eye

const int               L                      = 0;
const int               R                      = 1;
ID3D11RenderTargetView* g_pRenderTargetView[2] = {};
ID3D11Texture2D*        g_pDepthStencil[2]     = {};
ID3D11DepthStencilView* g_pDepthStencilView[2] = {};

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader*  g_pPixelShader  = nullptr;
ID3D11InputLayout*  g_pVertexLayout = nullptr;
ID3D11Buffer*       g_pVertexBuffer = nullptr;
ID3D11Buffer*       g_pIndexBuffer  = nullptr;

ID3D11Buffer* g_pSharedCB = nullptr;

XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

StereoHandle g_StereoHandle;

IDirect3DDevice9Ex*  g_device9Ex;
IDirect3DSwapChain9* g_swapChainDx9;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT          InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT          InitStereo();
HRESULT          CreateDX11Device();
HRESULT          InitDX11Device();
HRESULT          InitDX9Device();
HRESULT          ActivateStereo();
void             CleanupDevice();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void             RenderFrame();

//--------------------------------------------------------------------------------------
// Helper function to cleanup all those FAILED checks we don't care about.
// Copied from the DirectXTK.
//--------------------------------------------------------------------------------------
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        // Set a breakpoint on this line to catch DirectX API errors
        throw std::exception();
    }
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitStereo()))
        return 0;

    // DX9 setup first, so main window becomes full screen.
    if (FAILED(InitDX9Device()))
    {
        CleanupDevice();
        return 0;
    }

    // Setup DX11 before nvidia stereo api is called, so it is a regular environment,
    // which would match a game injection.
    if (FAILED(CreateDX11Device()))
    {
        CleanupDevice();
        return 0;
    }
    if (FAILED(InitDX11Device()))
    {
        CleanupDevice();
        return 0;
    }

    if (FAILED(ActivateStereo()))
    {
        CleanupDevice();
        return 0;
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            RenderFrame();
        }
    }

    CleanupDevice();

    return (int)msg.wParam;
}

//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
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
    RECT rc = { 0, 0, (LONG)g_ScreenWidth, (LONG)g_ScreenHeight };
    AdjustWindowRect(&rc, WS_BORDER, FALSE);
    g_hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"TutorialWindowClass", L"Direct3D 11 Tutorial 7", WS_BORDER, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd)
    {
        DWORD err = GetLastError();
        return err;
    }

    DWORD window_type = WS_OVERLAPPED;
    if (dx11_make_child)
        window_type = WS_CHILD;

    WNDCLASSEX wc;
    // clear out the window class for use
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    // fill in the struct with the needed information
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = DefWindowProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"ChildWindow";

    // register the window class
    RegisterClassEx(&wc);

    // Create a child window for the dx11 output to be invisible.
    {
        // Create a child window for the dx11 output to be invisible.
        g_child_hWnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, L"ChildWindow", nullptr, window_type, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, g_hWnd, nullptr, hInstance, nullptr);
    }
    if (!g_child_hWnd)
    {
        DWORD err = GetLastError();
        return err;
    }

    bool shown = IsWindowVisible(g_child_hWnd);
    shown      = IsWindowVisible(g_hWnd);

    //    shown      = ShowWindow(g_child_hWnd, SW_SHOWNA);  //If shown, DX11 present takes over.
    shown = ShowWindow(g_hWnd, nCmdShow);

    if (!dx11_window)
        g_child_hWnd = g_hWnd;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Setup nvapi, and enable stereo by direct mode for the app.
// This must be called before the Device is created for Direct Mode to work.
//--------------------------------------------------------------------------------------
HRESULT InitStereo()
{
    NvAPI_Status status;

    status = NvAPI_Initialize();
    if (FAILED(status))
        return status;

    // The entire point is to show stereo.
    // If it's not enabled in the control panel, let the user know.
    NvU8 stereoEnabled;
    status = NvAPI_Stereo_IsEnabled(&stereoEnabled);
    if (FAILED(status) || !stereoEnabled)
    {
        MessageBox(g_hWnd, L"3D Vision is not enabled. Enable it in the NVidia Control Panel.", L"Error", MB_OK);
        return status;
    }

    status = NvAPI_Stereo_SetDriverMode(NVAPI_STEREO_DRIVER_MODE_DIRECT);
    if (FAILED(status))
        return status;

    return status;
}

// Go full screen or back to windowed.
// Must reset both dx9 and dx11 swapchains.
//
// For DX9 3D, it's required that we run in exclusive full-screen mode, otherwise 3D
// Vision will not activate on 5xx drivers. On 452.06 windowed stereo works.

void ToggleFullScreen()
{
    HRESULT hr;

    // ----------- DX9

    D3DPRESENT_PARAMETERS present;
    hr = g_swapChainDx9->GetPresentParameters(&present);
    ThrowIfFailed(hr);

    present.Windowed = !present.Windowed;
    if (present.Windowed)
    {
        present.FullScreen_RefreshRateInHz = 0;
        present.BackBufferFormat           = D3DFMT_UNKNOWN;
    }

    D3DDISPLAYMODEEX fullscreen = {};
    fullscreen.Size             = sizeof(D3DDISPLAYMODEEX);
    fullscreen.Width            = present.BackBufferWidth;
    fullscreen.Height           = present.BackBufferHeight;
    fullscreen.RefreshRate      = present.FullScreen_RefreshRateInHz;
    fullscreen.Format           = present.BackBufferFormat;
    fullscreen.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;

    hr = g_device9Ex->ResetEx(&present, present.Windowed ? nullptr : &fullscreen);
    ThrowIfFailed(hr);

    // ----------- DX11

    /*
     * Don't do this sequence, because the dx11 swap chain does not need to
     * be full screen, and can stay at it's regular size.  If this goes into
     * full screen, it interferes with the primary window, becoming frontmost.
     */
    //BOOL win_state;
    //hr = g_pSwapChain->GetFullscreenState(&win_state, nullptr);
    //ThrowIfFailed(hr);

    //win_state = !win_state;

    //// Now that main window is full screen, reset the DX11 swapchain too.
    //hr = g_pSwapChain->SetFullscreenState(win_state, nullptr);
    //ThrowIfFailed(hr);

    //// DX11 reports that if we are using Flip_Sequential, that we must ResizeBuffers too.
    //// Setting everything to hard coded 2560x1440 for simplified testing.

    //// Clearing context state releases all its buffers.
    //g_pImmediateContext->ClearState();
    //g_pImmediateContext->Flush();
    //g_pRenderTargetView[L]->Release();
    //g_pRenderTargetView[R]->Release();
    //g_pDepthStencilView[L]->Release();
    //g_pDepthStencilView[R]->Release();

    ////// Report on any leftovers.
    ////ID3D11Debug* d3dDebug = nullptr;
    ////hr                    = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
    ////ThrowIfFailed(hr);

    ////hr = d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
    ////ThrowIfFailed(hr);
    ////d3dDebug->Release();

    //hr = g_pSwapChain->ResizeBuffers(0, g_ScreenWidth, g_ScreenHeight, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    //ThrowIfFailed(hr);

    // Rebuild drawing environment for DX11.
    //InitDX11Device();
}

//--------------------------------------------------------------------------------------
// Activate stereo for the given device.
// This must be called after the device is created.
//--------------------------------------------------------------------------------------
HRESULT ActivateStereo()
{
    NvAPI_Status status;

    status = NvAPI_Stereo_CreateHandleFromIUnknown(g_device9Ex, &g_StereoHandle);
    ThrowIfFailed(status);

    return status;
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr                   = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
            pErrorBlob->Release();
        }
        return hr;
    }
    if (pErrorBlob)
        pErrorBlob->Release();

    return S_OK;
}

// All of the init functions to be done to prepare for drawing. We need these separate
// so that we can rebuild this after changing to or from fullscreen.

HRESULT InitDX11Device()
{
    HRESULT hr;

    if (dx11_skip)
        return S_OK;

    // Create a render target view from the backbuffer
    // There are now two of these, each the same size as backbuffer.
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr                           = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    ThrowIfFailed(hr);

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView[L]);
    hr |= g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView[R]);
    pBackBuffer->Release();
    ThrowIfFailed(hr);

    // Create the depth stencil view
    //
    // This is not strictly necessary for our 3D, but is almost always used.
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format                        = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDSV.ViewDimension                 = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice            = 0;
    hr                                    = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil[0], &descDSV, &g_pDepthStencilView[0]);
    hr |= g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil[1], &descDSV, &g_pDepthStencilView[1]);
    ThrowIfFailed(hr);

    // This viewport is the screen width. Previous experiments were 2x width, but since
    // we are doing each eye buffers, we need single width.
    D3D11_VIEWPORT vp;
    vp.Width    = (FLOAT)g_ScreenWidth;
    vp.Height   = (FLOAT)g_ScreenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // First present is sometimes needed for init.  With DO_NOT_SEQUENCE
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView[L], Colors::Lavender);
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView[R], Colors::Plum);
    hr = g_pSwapChain->Present(0, 0);
    ThrowIfFailed(hr);

    return hr;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT CreateDX11Device()
{
    HRESULT hr = S_OK;

    if (dx11_skip)
        return S_OK;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Width                   = g_ScreenWidth;
    sd.BufferDesc.Height                  = g_ScreenHeight;
    sd.BufferDesc.RefreshRate.Numerator   = 120;  // Needs to be 120Hz for 3D Vision
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount                        = 2;  // Must be two or more

    sd.Windowed   = TRUE;  // Starts windowed, go fullscreen on alt-enter
    sd.SwapEffect = dx11_swap_mode;
    sd.Flags      = 0;

    sd.OutputWindow = g_child_hWnd;

    // Create the simple DX11, Device, SwapChain, and Context.
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);
    ThrowIfFailed(hr);

    // Create depth stencil texture

    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory(&descDepth, sizeof(descDepth));
    descDepth.Width              = g_ScreenWidth;
    descDepth.Height             = g_ScreenHeight;
    descDepth.MipLevels          = 1;
    descDepth.ArraySize          = 1;
    descDepth.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count   = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage              = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags     = 0;
    descDepth.MiscFlags          = 0;
    hr                           = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil[L]);
    hr |= g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil[R]);
    ThrowIfFailed(hr);

    // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    hr                = CompileShaderFromFile(L"Tutorial07.fx", "VS", "vs_4_0", &pVSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release();
    ThrowIfFailed(hr);

    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    hr                = CompileShaderFromFile(L"Tutorial07.fx", "PS", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release();
    ThrowIfFailed(hr);

    // Create vertex buffer for the cube
    SimpleVertex vertices[] = {
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
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) }
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(SimpleVertex) * 24;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices;
    hr               = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    ThrowIfFailed(hr);

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
    InitData.pSysMem  = indices;
    hr                = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    ThrowIfFailed(hr);

    // Create the constant buffer. This is used to update the resource on the
    // GPU so that the cube will animate.
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(SharedCB);
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr                = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pSharedCB);
    ThrowIfFailed(hr);

    // Initialize the world matrix
    g_World = XMMatrixIdentity();

    // Initialize the view matrix
    XMVECTOR Eye = XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f);
    XMVECTOR At  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR Up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_View       = XMMatrixLookAtLH(Eye, At, Up);

    // Initialize the projection matrix
    //
    // For the projection matrix, the shaders know nothing about being in stereo,
    // so this needs to be only ScreenWidth, one per eye.
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)g_ScreenWidth / (float)g_ScreenHeight, 0.01f, 100.0f);

    return S_OK;
}

// Setup a DX9 output device, that will be done via Direct Mode.
// SetDriverMode(DirectMode) must already be done.

HRESULT InitDX9Device()
{
    HRESULT       hr;
    IDirect3D9Ex* d3d9Ex;

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9Ex);
    ThrowIfFailed(hr);

    D3DPRESENT_PARAMETERS d3dpp = {};

    d3dpp.BackBufferWidth      = g_ScreenWidth;
    d3dpp.BackBufferHeight     = g_ScreenHeight;
    d3dpp.BackBufferFormat     = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferCount      = 1;
    d3dpp.SwapEffect           = dx9_swap_mode;
    d3dpp.hDeviceWindow        = g_hWnd;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    //d3dpp.Windowed                   = FALSE;
    //d3dpp.FullScreen_RefreshRateInHz = 120;

    d3dpp.Windowed                   = TRUE;
    d3dpp.FullScreen_RefreshRateInHz = 0;

    D3DDISPLAYMODEEX fullscreen = {};
    fullscreen.Size             = sizeof(D3DDISPLAYMODEEX);
    fullscreen.Width            = g_ScreenWidth;
    fullscreen.Height           = g_ScreenHeight;
    fullscreen.RefreshRate      = 120;
    fullscreen.Format           = D3DFMT_A8R8G8B8;
    fullscreen.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;

    // create the DX9 device we can use for Direct Mode output
    // For full screen use. Doing this at init time goes immediately to full
    // screen mode, so we don't have resize buffers or Reset.
    // Required hFocusWindow param, even if specified in d3dpp. Otherwise
    // it will not go into full screen mode and switch monitor resolution.
    hr = d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, d3dpp.Windowed ? nullptr : &fullscreen, &g_device9Ex);
    ThrowIfFailed(hr);

    hr = g_device9Ex->GetSwapChain(0, &g_swapChainDx9);
    ThrowIfFailed(hr);

    d3d9Ex->Release();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
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
    if (g_pDepthStencil[L])
        g_pDepthStencil[L]->Release();
    if (g_pDepthStencilView[L])
        g_pDepthStencilView[L]->Release();
    if (g_pRenderTargetView[L])
        g_pRenderTargetView[L]->Release();
    if (g_pDepthStencil[R])
        g_pDepthStencil[R]->Release();
    if (g_pDepthStencilView[R])
        g_pDepthStencilView[R]->Release();
    if (g_pRenderTargetView[R])
        g_pRenderTargetView[R]->Release();

    if (g_pSwapChain)
        g_pSwapChain->Release();
    if (g_pImmediateContext)
        g_pImmediateContext->Release();
    if (g_pd3dDevice)
        g_pd3dDevice->Release();

    if (g_StereoHandle)
        NvAPI_Stereo_DestroyHandle(g_StereoHandle);
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    if (hWnd == g_child_hWnd)
        DebugBreak();

    switch (message)
    {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_SIZE:
            //ToggleFullScreen();
            break;

        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN)
                ToggleFullScreen();
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            break;

            // Note that this tutorial does not handle resizing (WM_SIZE) requests,
            // so we created the window without the resize border.

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Render current image, use eye specific RenderTargetView.
//--------------------------------------------------------------------------------------
void Render(int eye)
{
    //
    // Clear the back buffer
    //
    // Even though this uses the g_pRenderTargetView, it only affects half the backbuffer,
    // because we have set a specific eye.
    //
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView[eye], Colors::MintCream);

    //
    // Clear the depth buffer to 1.0 (max depth)
    //
    // Also done on a per-eye basis.
    //
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView[eye], D3D11_CLEAR_DEPTH, 1.0f, 0);

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView[eye], g_pDepthStencilView[eye]);

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

//--------------------------------------------------------------------------------------
// Render a frame, both eyes.
//--------------------------------------------------------------------------------------
void RenderFrame()
{
    SharedCB cb;
    HRESULT  hr;

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
    //NvAPI_Status status;
    //float pConvergence;
    //float pSeparationPercentage;
    //float pEyeSeparation;

    //status = NvAPI_Stereo_GetConvergence(g_StereoHandle, &pConvergence);
    //status = NvAPI_Stereo_GetSeparation(g_StereoHandle, &pSeparationPercentage);
    //status = NvAPI_Stereo_GetEyeSeparation(g_StereoHandle, &pEyeSeparation);

    //float separation = pEyeSeparation * pSeparationPercentage / 100;
    //float convergence = pEyeSeparation * pSeparationPercentage / 100 * pConvergence;

    //float        separation  = 0.2f;
    //float        convergence = 1.0f;
    //
    // Drawing same object twice, once for each eye, into each eye buffer.
    // Eye specific setup is for the Projection matrix.
    // The _31 parameter is the X translation for the off center Projection.
    // The _41 parameter, I don't presently know what it is, but this
    // sequence works to handle both convergence and separation hot keys properly.
    //

    if (!dx11_skip)
    {
        {
            g_World *= XMMatrixTranslation(0.05f, 0.0f, 0.0f);

            cb.mWorld      = XMMatrixTranspose(g_World);
            cb.mView       = XMMatrixTranspose(g_View);
            cb.mProjection = XMMatrixTranspose(g_Projection);
            g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

            Render(L);
        }

        {
            g_World *= XMMatrixTranslation(-0.05f, 0.0f, 0.0f);

            cb.mWorld      = XMMatrixTranspose(g_World);
            cb.mView       = XMMatrixTranspose(g_View);
            cb.mProjection = XMMatrixTranspose(g_Projection);
            g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

            Render(R);
        }
    }

    //
    // Present dx11 side
    //
    if (dx11_present_first && !dx11_skip)
    {
        hr = g_pSwapChain->Present(dx11_sync_interval, dx11_flags);
        if (hr != 0x887a000a)  // Busy is OK
            ThrowIfFailed(hr);
    }

    hr = g_device9Ex->BeginScene();
    ThrowIfFailed(hr);
    {
        NvAPI_Status status = NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_LEFT);
        g_device9Ex->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(0.3, 0, 0, 1), 0, 0);  // dark red
        status = NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_RIGHT);
        g_device9Ex->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(0, 0.3, 0, 1), 0, 0);  // dark green
    }
    hr = g_device9Ex->EndScene();
    ThrowIfFailed(hr);

    // Now Present via the DX9 device as well. This will be the one actually showing.
    hr = g_device9Ex->PresentEx(nullptr, nullptr, nullptr, nullptr, dx9_flags);
    if (hr != 0x8876021c)  // Still drawing is OK
        ThrowIfFailed(hr);

    if (!dx11_present_first && !dx11_skip)
    {
        hr = g_pSwapChain->Present(dx11_sync_interval, dx11_flags);
        if (hr != 0x887a000a)  // Busy is OK
            ThrowIfFailed(hr);
    }
}
