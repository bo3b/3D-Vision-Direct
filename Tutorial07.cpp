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
UINT         g_ScreenWidth  = 2560;
UINT         g_ScreenHeight = 1440;

IDirect3D9Ex*       g_d3d9Ex;
IDirect3DDevice9Ex* g_device9Ex;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT          InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT          InitStereo();
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

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    // Setup DX11 before nvidia stereo api is called, so it is a regular environment,
    // which would match a game being hacked.
    if (FAILED(InitDX11Device()))
    {
        CleanupDevice();
        return 0;
    }

    if (FAILED(InitStereo()))
        return 0;

    if (FAILED(InitDX9Device()))
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
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);

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

//--------------------------------------------------------------------------------------
// Activate stereo for the given device.
// This must be called after the device is created.
//--------------------------------------------------------------------------------------
HRESULT ActivateStereo()
{
    NvAPI_Status status;

    status = NvAPI_Stereo_CreateHandleFromIUnknown(g_device9Ex, &g_StereoHandle);
    if (FAILED(status))
        return status;

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

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDX11Device()
{
    HRESULT hr = S_OK;

    // Create a child window for the dx11 output to be invisible.
    g_child_hWnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, L"TutorialWindowClass", nullptr, WS_CHILD, 0, 0, g_ScreenWidth, g_ScreenHeight, g_hWnd, nullptr, nullptr, nullptr);
    if (!g_child_hWnd)
    {
        DWORD err = GetLastError();
        return E_FAIL;
    }

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
    sd.BufferCount                        = 2;
    sd.OutputWindow                       = g_child_hWnd;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.Flags                              = 0;

    // Create the simple DX11, Device, SwapChain, and Context.
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);
    ThrowIfFailed(hr);

    // For DX11 3D, it's required that we run in exclusive full-screen mode, otherwise 3D
    // Vision will not activate.
    //hr = g_pSwapChain->SetFullscreenState(TRUE, nullptr);
    //    ThrowIfFailed(hr);
    //    return hr;

    // DX11 reports that if we are using Flip_Sequential, that we must ResizeBuffers too.
    // Setting everything to hard coded 2560x1440 for simplified testing.
    //hr = g_pSwapChain->ResizeBuffers(2, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    //    ThrowIfFailed(hr);
    //    return hr;

    // Create a render target view from the backbuffer
    // There are now two of these, each the same size as backbuffer.
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr                           = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    ThrowIfFailed(hr);

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView[L]);
    hr |= g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView[R]);
    pBackBuffer->Release();
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

    // Create the depth stencil view
    //
    // This is not strictly necessary for our 3D, but is almost always used.
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format             = descDepth.Format;
    descDSV.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr                         = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil[0], &descDSV, &g_pDepthStencilView[0]);
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

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

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
    ThrowIfFailed(hr);

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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

    // First present is sometimes needed for init.
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView[L], Colors::LimeGreen);
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView[R], Colors::LimeGreen);
    hr = g_pSwapChain->Present(0, 0);
    ThrowIfFailed(hr);

    return S_OK;
}

// Setup a DX9 output device, that will be done via Direct Mode.
// SetDriverMode(DirectMode) must already be done.

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

HRESULT InitDX9Device()
{
    HRESULT hr;

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &g_d3d9Ex);
    ThrowIfFailed(hr);

    D3DPRESENT_PARAMETERS d3dpp;

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.BackBufferWidth  = g_ScreenWidth;
    d3dpp.BackBufferHeight = g_ScreenHeight;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.BackBufferCount  = 1;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_FLIP;
    d3dpp.hDeviceWindow    = g_hWnd;
    d3dpp.Windowed         = TRUE;
    //d3dpp.EnableAutoDepthStencil = TRUE;
    //d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    // create the DX9 device we can use for Direct Mode output

    hr = g_d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, nullptr, &g_device9Ex);
    ThrowIfFailed(hr);

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

    switch (message)
    {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
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
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView[eye], Colors::MidnightBlue);

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

    //NvAPI_Status status = NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_LEFT);
    //if (SUCCEEDED(status))
    {
        g_World *= XMMatrixTranslation(0.05f, 0.0f, 0.0f);

        cb.mWorld      = XMMatrixTranspose(g_World);
        cb.mView       = XMMatrixTranspose(g_View);
        cb.mProjection = XMMatrixTranspose(g_Projection);
        g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

        Render(L);
    }

    //status = NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_RIGHT);
    //if (SUCCEEDED(status))
    {
        g_World *= XMMatrixTranslation(-0.05f, 0.0f, 0.0f);

        cb.mWorld      = XMMatrixTranspose(g_World);
        cb.mView       = XMMatrixTranspose(g_View);
        cb.mProjection = XMMatrixTranspose(g_Projection);
        g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

        Render(R);
    }
    //
    // Present our back buffer to our front buffer
    //
    // In stereo mode, the driver knows to use the 2x width buffer, and
    // present each eye in order.
    //
    hr = g_pSwapChain->Present(1, DXGI_PRESENT_DO_NOT_SEQUENCE);
    ThrowIfFailed(hr);

    hr = g_device9Ex->BeginScene();
    ThrowIfFailed(hr);
    {
        NvAPI_Status status = NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_LEFT);
        g_device9Ex->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(0.7, 0, 0, 1), 0, 0);  // Red
        status = NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_RIGHT);
        g_device9Ex->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(0, 0, 0.7, 1), 0, 0);  // Blue
    }
    hr = g_device9Ex->EndScene();
    ThrowIfFailed(hr);

    // Now Present via the DX9 device as well. This will be the one actually showing.
    hr = g_device9Ex->PresentEx(nullptr, nullptr, g_hWnd, nullptr, D3DPRESENT_INTERVAL_IMMEDIATE);
    ThrowIfFailed(hr);
}
