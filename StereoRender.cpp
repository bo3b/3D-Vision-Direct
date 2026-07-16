#include "StereoRender.h"

#include "Globals.h"
#include "Utils.h"
#include "DirectXMath.h"
#include "DirectxColors.h"

#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgiformat.h>
#include <d3d11_4.h>
#include <d3d11sdklayers.h>
#include <d3dcommon.h>
#include <Windows.h>
#include <wrl/client.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <ios>
#include <ostream>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>
#include <cassert>

using namespace DirectX;

// This is sort of 'the game' that would be injected. Drawing environment
// based on DX11 that we can't directly modify, but can tweak params.

ID3D11Device* gameDevice = nullptr;

ComPtr<ID3D11Texture2D>        g_LR_tex;
ComPtr<ID3D11RenderTargetView> g_LR_RTV;

ID3D11VertexShader*   g_pVertexShader   = nullptr;
ID3D11GeometryShader* g_pGeometryShader = nullptr;
ID3D11PixelShader*    g_pPixelShader    = nullptr;
ID3D11InputLayout*    g_pVertexLayout   = nullptr;
ID3D11Buffer*         g_pVertexBuffer   = nullptr;
ID3D11Buffer*         g_pIndexBuffer    = nullptr;

ID3D11Buffer* g_pSharedCB = nullptr;

XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

float g_bar_x = 0.0f;  // Judder test bar position, left to right

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
    UINT     EyeIndex;  // Selects the g_LR_RTV array slice: 0 = left, 1 = right.
    UINT     pad[3];    // Constant buffers must be a multiple of 16 bytes.
};

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
//
//  This is 'Game' device and swapchain as a simulation of the game creating these.
//  In geo-11 we will capture these by the proxy layer, but don't 'own' them.
//--------------------------------------------------------------------------------------
HRESULT init_dx11(HWND game_window)
{
    HRESULT hr;

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
    desc.OutputWindow                       = game_window;
    desc.SampleDesc.Count                   = 1;
    desc.SampleDesc.Quality                 = 0;
    desc.Windowed                           = TRUE;  // Start windowed, then switch to fullscreen exclusive.
    desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    desc.SwapEffect                         = g_swap_effect;  // Allows windowed 3D.

    // Create the simple DX11, Device, SwapChain, and Context.
    HR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_device_flags, nullptr, 0, D3D11_SDK_VERSION, &desc, &g_GameSwapChain, &gameDevice, nullptr, &g_GameImmediateContext));
    g_out << "init_dx11 CreateDeviceAndSwapChain to render game_window. SwapChain: " << g_GameSwapChain << endlog;

#ifdef _DEBUG
    // Break into the debugger the moment the Debug Layer reports an error.
    {
        ComPtr<ID3D11InfoQueue> info_queue;
        HR(gameDevice->QueryInterface(__uuidof(ID3D11InfoQueue), reinterpret_cast<void**>(info_queue.GetAddressOf())));
        HR(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
        HR(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
    }
#endif

    // Create the offscreen Texture2D for both eyes that we will DrawIndexed into.
    // They need to be identical to the drawing backbuffer in size and color format.
    {
        ComPtr<ID3D11Texture2D> drawing_backbuffer;
        HR(g_GameSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(drawing_backbuffer.GetAddressOf())));

        D3D11_TEXTURE2D_DESC texture_desc;
        drawing_backbuffer->GetDesc(&texture_desc);  // Match backbuffer specs
        texture_desc.ArraySize = 2;                  // Left and Right eye

        HR(gameDevice->CreateTexture2D(&texture_desc, nullptr, &g_LR_tex));

        // RenderTargetViews are expensive to make, so we need to make them in advance.
        // This will be two slice array, used in VS.
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc  = {};
        rtv_desc.Format                         = texture_desc.Format;
        rtv_desc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.MipSlice        = 0;
        rtv_desc.Texture2DArray.ArraySize       = 2;
        rtv_desc.Texture2DArray.FirstArraySlice = 0;
        HR(gameDevice->CreateRenderTargetView(g_LR_tex.Get(), &rtv_desc, &g_LR_RTV));

        // Duplicate copy of output textures for staging.
        HR(gameDevice->CreateTexture2D(&texture_desc, nullptr, &g_game_latest_LR));

        ComPtr<ID3D11Multithread> multi_thread;
        g_GameImmediateContext->QueryInterface(_uuidof(ID3D11Multithread), &multi_thread);
        multi_thread->SetMultithreadProtected(true);
    }

    // Default wide open viewport
    D3D11_VIEWPORT vp;
    vp.Width    = (FLOAT)g_ScreenWidth;
    vp.Height   = (FLOAT)g_ScreenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_GameImmediateContext->RSSetViewports(1, &vp);

    // Compile the vertex shader
    ID3DBlob* vs_blob = nullptr;
    hr                = compile_shader_from_file(L"Tutorial07.fx", "VS", "vs_4_0", &vs_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the vertex shader
    hr = gameDevice->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr))
    {
        vs_blob->Release();
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = ARRAYSIZE(layout);

    // Create the input layout
    hr = gameDevice->CreateInputLayout(layout, num_elements, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &g_pVertexLayout);
    vs_blob->Release();
    if (FAILED(hr))
        return hr;

    // Compile the pixel shader
    ID3DBlob* ps_blob = nullptr;
    hr                = compile_shader_from_file(L"Tutorial07.fx", "PS", "ps_4_0", &ps_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = gameDevice->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &g_pPixelShader);
    ps_blob->Release();
    if (FAILED(hr))
        return hr;

    // Compile and create the geometry shader. Only job is to stamp
    // SV_RenderTargetArrayIndex from EyeIndex, so each eye's draw lands in
    // its own slice of g_LR_RTV.
    ID3DBlob* gs_blob = nullptr;
    hr                = compile_shader_from_file(L"Tutorial07.fx", "GS", "gs_4_0", &gs_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    hr = gameDevice->CreateGeometryShader(gs_blob->GetBufferPointer(), gs_blob->GetBufferSize(), nullptr, &g_pGeometryShader);
    gs_blob->Release();
    if (FAILED(hr))
        return hr;

    // Set the input layout
    g_GameImmediateContext->IASetInputLayout(g_pVertexLayout);

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

    hr = gameDevice->CreateBuffer(&bd, &init_data, &g_pVertexBuffer);
    if (FAILED(hr))
        return hr;

    // Set vertex buffer
    UINT stride = sizeof(simple_vertex);
    UINT offset = 0;
    g_GameImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

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
    hr                = gameDevice->CreateBuffer(&bd, &init_data, &g_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    // Set index buffer
    g_GameImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_GameImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffer
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(shared_CB);
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr                = gameDevice->CreateBuffer(&bd, nullptr, &g_pSharedCB);
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
// Render current image, eye independent. eye_cb is this eye's projection setup;
// updated here (not by the caller) because the load pass below also writes b0.
//--------------------------------------------------------------------------------------
void draw_cube(bool rightEye, const shared_CB& eye_cb)
{
    // Set the RenderTargetView for both RTV slices.
    ID3D11RenderTargetView* rtv_array[] = { g_LR_RTV.Get() };
    g_GameImmediateContext->OMSetRenderTargets(1, rtv_array, nullptr);

    //
    // Render the cube
    //
    // Projection matrix in g_pSharedCB determines eye view. Re-uploaded here
    // because the load pass above may have overwritten b0. EyeIndex tells the
    // GS which g_LR_RTV slice this draw belongs to.
    //
    shared_CB cb = eye_cb;
    cb.EyeIndex  = rightEye ? 1 : 0;
    g_GameImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);
    g_GameImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_GameImmediateContext->VSSetConstantBuffers(0, 1, &g_pSharedCB);
    g_GameImmediateContext->GSSetShader(g_pGeometryShader, nullptr, 0);
    g_GameImmediateContext->GSSetConstantBuffers(0, 1, &g_pSharedCB);
    g_GameImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_GameImmediateContext->DrawIndexed(36, 0, 0);

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
        bar_cb.EyeIndex    = cb.EyeIndex;
        g_GameImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &bar_cb, 0, 0);
        g_GameImmediateContext->DrawIndexed(36, 0, 0);
    }
}

//--------------------------------------------------------------------------------------
// Main loop calls to render_frame.  Like Present from a game.
//--------------------------------------------------------------------------------------
void render_frame()
{
    Sleep(g_framerate);  // pretend GPU work

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

    // Checking for possible fatal errors that could cause an eye swap situation.
    // Does not seem to ever hit exception handler, which is what we'd expect.
    try
    {
        // Clear both eye slices once, up front. g_LR_RTV covers both slices, so
        // a single clear resets the whole pair; clearing per eye inside
        // draw_cube would erase the slice the previous eye just drew.
        g_GameImmediateContext->ClearRenderTargetView(g_LR_RTV.Get(), Colors::OliveDrab);

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
    }
    catch (const std::exception& e)
    {
        g_out << "!!!  render_frame exception: " << e.what() << endlog;
        DebugBreak();
    }
    catch (...)
    {
        g_out << "!!!  Unknown render_frame exception: " << endlog;
        DebugBreak();
    }

    // Publish the pair to the presenter.
    copy_to_handoff();
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void cleanup_device()
{
    if (g_GameSwapChain)
        g_GameSwapChain->SetFullscreenState(FALSE, nullptr);

    if (g_GameImmediateContext)
        g_GameImmediateContext->ClearState();

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
    if (g_pGeometryShader)
        g_pGeometryShader->Release();
    if (g_pPixelShader)
        g_pPixelShader->Release();

    // ComPtrs don't need manual cleanup.

    if (g_GameSwapChain)
        g_GameSwapChain->Release();
    if (g_GameImmediateContext)
        g_GameImmediateContext->Release();
    if (gameDevice)
        gameDevice->Release();
}

// When we hit F4, we want to toggle between windowed and exclusive fullscreen.
void fullscreen(bool windowed)
{
    // Stop the Output display loop, because we are reseting the output
    // backbuffer. This waits for thread to exit.
    g_Display->StopRefresh();

    // ResizeBuffers below requires that the swapchain's backbuffers have no
    // outstanding references and that nothing referencing them is still queued
    // on the context. copy_to_handoff already releases its backbuffer each
    // frame; here we unbind any bound render target (so a stale RTV can't hold
    // one) and flush pending GPU work before the resize.
    g_GameImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    g_GameImmediateContext->Flush();

    HRESULT hr = g_GameSwapChain->SetFullscreenState(!windowed, NULL);
    if (FAILED(hr))
    {
        g_out << "SetFullscreenState failed: " << std::hex << hr << std::dec << endlog;
        DebugBreak();
    }

    // Because we use a FLIP swap effect we need to Resize buffers too.
    HR(g_GameSwapChain->ResizeBuffers(g_bufferCount, g_ScreenWidth, g_ScreenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    // With resized buffers we can now resume output display.
    g_Display->StartRefresh();
}

//--------------------------------------------------------------------------------------
// Game side of the geo-11 handoff (CopyToHandoff port): publish the just-rendered
// L/R pair into the next FIFO slot under its keyed mutex, with the readiness
// fence signaled behind the copies. Never wedges the game thread: a 150ms cap
// means a hung presenter costs one dropped frame, not a hang.
//--------------------------------------------------------------------------------------
void copy_to_handoff()
{
    // Call through to game's hooked Present to keep hooks happy.
    // With SyncInterval=0, it will be discarded
    HR(g_GameSwapChain->Present(0, 0));

    // Copy both eyes into storage for the Latest Frame from the Game.
    // This copy is available for the monitor display to pick up.

    {
        g_GameImmediateContext->CopyResource(g_game_latest_LR.Get(), g_LR_tex.Get());  // both eyes
    }
}
