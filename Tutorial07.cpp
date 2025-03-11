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
//   just hit setLeftEye at the top of the loop, and it will auto-swap to right during
//   a given frame, even with no call. This runs for at least 6 frames, and maybe more.
//	 There is an auto-timeout of some form, where if does not get any AA setEye commands
//   it will stop running and turn off the bright green and infra-red to the glasses.
//
//   It's not at all clear why we get eye swaps, especially because I am sure to always
//   call setLeftEye before drawing left eye data. So it also does not respect the
//   setEye command, and uses it as a way to resync it's timer to avoid drift, but
//   does not actually immediately switch eyes.  There is the $40 clear command, but
//   that also seems to do nothing. It does not restart the device in proper mode. I
//   removed the 'Read' commands, because I don't think we are about the front button
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
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

#include "nvapi.h"
#include "Timer.h"
#include "nvidiaShutterGlasses.h"
#include <thread>

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT          InitWindow(HINSTANCE hInstance, int nCmdShow);
void             StartGlasses();
void             EnableLightBoost();
HRESULT          InitDevice();
void             CleanupDevice();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void             RenderFrame();
void             Render();

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
HINSTANCE g_hInst = nullptr;
HWND      g_hWnd  = nullptr;

ID3D11Device*        g_pd3dDevice        = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain*      g_pSwapChain        = nullptr;

ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
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

LONG g_ScreenWidth  = 1280;
LONG g_ScreenHeight = 720;

Timer              g_Timer;
double             g_lastFrame = 0;
std::ostringstream g_out;

NvidiaShutterGlasses g_shutterGlasses;
std::atomic<bool>    g_running(false);
std::thread          g_renderThread;

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Before we create DX11 and windows, enable LightBoost.
    EnableLightBoost();
    // Safely Wake and Initialize the timing of the emitter.
    StartGlasses();

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return 0;
    }

    // Start a rendering subthread, so that rendering is off the main app thread,
    // and thus UI things like dragging the window don't block drawing.
    g_running      = true;
    g_renderThread = std::thread(Render);

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
                g_shutterGlasses.toggleEyes();
            }
        }
    }

    // Cleanly stop drawing thread upon exit
    g_running = false;
    if (g_renderThread.joinable())
    {
        g_renderThread.join();  // Wait for it to cleanly exit.
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
    wcex.lpszClassName = "TutorialWindowClass";
    wcex.hIconSm       = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, g_ScreenWidth, g_ScreenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow("TutorialWindowClass", "Direct3D 11 Tutorial 7", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);

    g_Timer.Start();
    g_out << std::fixed << std::setprecision(2);

    return S_OK;
}

void EnableLightBoost()
{
    NvAPI_Status status;

    status = g_shutterGlasses.getCurrentResolution_NVIDIA();
    if (status != NVAPI_OK)
    {
        g_out << "!!! Fail !!!" << std::endl
              << " Unable to fetch current resolution and timing. " << std::endl
              << "!!! Fail !!!" << std::endl;
        OutputDebugStringA(g_out.str().c_str());
        exit(-1);
    }

    // Since we could get the resolution successfully, let's go ahead and enable
    // LightBoost.  We'll not error out if it fails to setup.
    status = g_shutterGlasses.enable_LightBoost_NVIDIA();
    if (status != NVAPI_OK)
    {
        g_out << "!!! Fail !!!" << std::endl
              << " Unable to enable timing for LightBoost. " << std::endl
              << "!!! Fail !!!" << std::endl;
        OutputDebugStringA(g_out.str().c_str());
    }
}

void StartGlasses()
{
    // Start timers and initialize the emitter timing values.
    g_shutterGlasses.WakeEmitter();
    g_shutterGlasses.refresh();

    // Start with left eye open.
    g_shutterGlasses.setLeftEye();
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
            OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
            pErrorBlob->Release();
        }
        return hr;
    }
    if (pErrorBlob)
        pErrorBlob->Release();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = g_ScreenWidth;
    sd.BufferDesc.Height                  = g_ScreenHeight;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 120;  // Needs to be 120Hz for 3D Vision emitter
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = g_hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    // Create the simple DX11, Device, SwapChain, and Context.
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);
    if (FAILED(hr))
        return hr;

    // For DX11 3D, it's required that we run in exclusive full-screen mode, otherwise 3D
    // Vision will not activate.
    //hr = g_pSwapChain->SetFullscreenState(TRUE, nullptr);
    //if (FAILED(hr))
    //	return hr;

    // Create a render target view from the backbuffer
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr                           = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

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
    hr                           = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
    if (FAILED(hr))
        return hr;

    // Create the depth stencil view
    //
    // This is not strictly necessary for our 3D, but is almost always used.
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format             = descDepth.Format;
    descDSV.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr                         = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr))
        return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

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
    ID3DBlob* pVSBlob = nullptr;
    hr                = CompileShaderFromFile(L"Tutorial07.fx", "VS", "vs_4_0", &pVSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
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
    if (FAILED(hr))
        return hr;

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    hr                = CompileShaderFromFile(L"Tutorial07.fx", "PS", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
        return hr;

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
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
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
    if (FAILED(hr))
        return hr;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
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
    InitData.pSysMem  = indices;
    hr                = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffer
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(SharedCB);
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr                = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pSharedCB);
    if (FAILED(hr))
        return hr;

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
    if (g_pDepthStencil)
        g_pDepthStencil->Release();
    if (g_pDepthStencilView)
        g_pDepthStencilView->Release();
    if (g_pRenderTargetView)
        g_pRenderTargetView->Release();

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
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
            // Reinit the emitter upon regaining focus.
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                g_out << "--> Deactivate  time: " << g_Timer.GetElapsedMicroseconds() / 1000.0f << " now: " << now.QuadPart << std::endl;
                OutputDebugStringA(g_out.str().c_str());
            }
            else
            {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                g_out << "<-- Activate    time: " << g_Timer.GetElapsedMicroseconds() / 1000.0f << " now: " << now.QuadPart << std::endl;
                OutputDebugStringA(g_out.str().c_str());
                //Sleep(5000);
                //	g_shutterGlasses.refresh();
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
void DrawCube()
{
    //
    // Clear the back buffer
    //
    // Even though this uses the g_pRenderTargetView, it only affects half the backbuffer,
    // because we have set a specific eye.
    //
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);

    //
    // Clear the depth buffer to 1.0 (max depth)
    //
    // Also done on a per-eye basis.
    //
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

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

void SleepMicroseconds(int64_t microseconds)
{
    LARGE_INTEGER frequency, start, current;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    int64_t targetTicks = (microseconds * frequency.QuadPart) / 1000000;

    do
    {
        QueryPerformanceCounter(&current);
    } while (current.QuadPart - start.QuadPart < targetTicks);
}

int64_t stall     = 0;
int     out_limit = 4;

//--------------------------------------------------------------------------------------
// Render a frame, both eyes.
//--------------------------------------------------------------------------------------
void RenderFrame()
{
    HRESULT hr;

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
    SharedCB cb;
    float    pConvergence;
    float    pSeparationPercentage;
    float    pEyeSeparation;

    pEyeSeparation        = 10.10f;
    pConvergence          = 4.0f;
    pSeparationPercentage = 0.52f;

    float separation  = pEyeSeparation * pSeparationPercentage / 100;
    float convergence = pEyeSeparation * pSeparationPercentage / 100 * pConvergence;

    //stall += 10;
    //SleepMicroseconds(stall);

    // Checking for possible fatal errors that could cause an eye swap situation.
    // Does not seem to ever hit exception handler, which is what we'd expect.
    try
    {
        // <----------------------- Left Eye -------------------------------
        //
        // Specifically set the LeftEye as active, not just toggle. This seems
        // to help get proper sync when the app is active, but doesn't help with
        // alt-tab eye swaps.
        double leftEyeStart = g_Timer.GetElapsedMicroseconds();

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

            DrawCube();
        }
        hr = g_pSwapChain->Present(1, 0);
        g_shutterGlasses.toggleEyes();
        if (FAILED(hr))
        {
            g_out << "Present failed: " << hr << std::endl;
            OutputDebugStringA(g_out.str().c_str());
            DebugBreak();
        }

        double leftEyeElapsed = (g_Timer.GetElapsedMicroseconds() - leftEyeStart) / 1000.0f;
        if (leftEyeElapsed > 18.0f)
        {
            g_out << "!! Left frame dropped. Eye swap." << std::endl;
            OutputDebugStringA(g_out.str().c_str());
            out_limit = 2;
            //g_shutterGlasses.refresh();	// re-init on drops
        }
        if (out_limit > 0)
        {
            g_out << "Left eye frame time:  " << leftEyeElapsed << " ms" << std::endl;
            OutputDebugStringA(g_out.str().c_str());
        }

        // <----------------------- Right Eye -------------------------------
        //
        // After eye-swaps, this surprisingly does nothing.
        //g_shutterGlasses.toggleEyes((int)0xffff0000);
        double rightEyeStart = g_Timer.GetElapsedMicroseconds();

        {
            cb.mWorld = XMMatrixTranspose(g_World);
            cb.mView  = XMMatrixTranspose(g_View);

            cb.mProjection = g_Projection;
            cb.mProjection._31 += separation;
            cb.mProjection._41 = -convergence;
            cb.mProjection     = XMMatrixTranspose(cb.mProjection);
            g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

            DrawCube();
        }
        hr = g_pSwapChain->Present(1, 0);
        g_shutterGlasses.toggleEyes();
        if (FAILED(hr))
        {
            g_out << "Present failed: " << hr << std::endl;
            OutputDebugStringA(g_out.str().c_str());
            DebugBreak();
        }

        double rightEyeElapsed = (g_Timer.GetElapsedMicroseconds() - rightEyeStart) / 1000.0f;
        ;
        if (rightEyeElapsed > 18.0f)
        {
            g_out << "!! Right frame dropped. Eye swap." << std::endl;
            OutputDebugStringA(g_out.str().c_str());
            out_limit = 2;
            //g_shutterGlasses.refresh();	// re-init on drops
        }
        if (out_limit > 0)
        {
            g_out << "Right eye frame time: " << rightEyeElapsed << " ms" << std::endl;
            OutputDebugStringA(g_out.str().c_str());
        }

        double currentFrame = g_Timer.GetElapsedMicroseconds();
        if (out_limit > 0)
        {
            g_out << "  full frame time:             " << (currentFrame - g_lastFrame) / 1000.0f << " ms" << std::endl;
            OutputDebugStringA(g_out.str().c_str());

            out_limit--;
        }
        g_lastFrame = currentFrame;
    }
    catch (const std::exception& e)
    {
        g_out << "!!!  RenderFrame exception: " << e.what() << std::endl;
        OutputDebugStringA(g_out.str().c_str());
        DebugBreak();
    }
    catch (...)
    {
        g_out << "!!!  Unknown RenderFrame exception: " << std::endl;
        OutputDebugStringA(g_out.str().c_str());
        DebugBreak();
    }
}

//--------------------------------------------------------------------------------------
// // Render call from the subthread.
//--------------------------------------------------------------------------------------
void Render()
{
    while (g_running)
    {
        RenderFrame();
    }
}
