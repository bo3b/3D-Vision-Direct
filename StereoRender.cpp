#include "StereoRender.h"

#include "Params.h"
#include "Utils.h"
#include "DirectXMath.h"
#include "DirectxColors.h"

#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgiformat.h>
#include <d3d11.h>
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

ID3D11Device*          g_pd3dDevice        = nullptr;
ID3D11DeviceContext*   g_pImmediateContext = nullptr;
IDXGISwapChain*        g_pSwapChain        = nullptr;
ComPtr<IDXGISwapChain> g_refresh_swapchain;

ComPtr<ID3D11Texture2D>        g_LR_tex;
ComPtr<ID3D11RenderTargetView> g_LR_RTV;

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
//--------------------------------------------------------------------------------------
HRESULT init_dx11(HWND window)
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
    desc.OutputWindow                       = window;  // 'Game' window
    desc.SampleDesc.Count                   = 1;
    desc.SampleDesc.Quality                 = 0;
    desc.Windowed                           = TRUE;  // Start windowed, then switch to fullscreen exclusive.
    desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

    // Create the simple DX11, Device, SwapChain, and Context.
    HR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_device_flags, nullptr, 0, D3D11_SDK_VERSION, &desc, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext));
    g_out << "init_dx11 CreateDeviceAndSwapChain for hidden render window. SwapChain: " << g_pSwapChain << std::endl;
    log();

    // Create the offscreen Texture2D for both eyes that we will DrawIndexed into.
    // They need to be identical to the drawing backbuffer in size and color format.
    {
        ComPtr<ID3D11Texture2D> drawing_backbuffer;
        HR(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(drawing_backbuffer.GetAddressOf())));

        D3D11_TEXTURE2D_DESC texture_desc;
        drawing_backbuffer->GetDesc(&texture_desc);  // Match backbuffer specs
        texture_desc.ArraySize = 2;                  // Left and Right eye

        HR(g_pd3dDevice->CreateTexture2D(&texture_desc, nullptr, &g_LR_tex));

        // RenderTargetViews are expensive to make, so we need to make them in advance.
        // This will be two slice array, used in VS.
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc  = {};
        rtv_desc.Format                         = texture_desc.Format;
        rtv_desc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.MipSlice        = 0;
        rtv_desc.Texture2DArray.ArraySize       = 2;
        rtv_desc.Texture2DArray.FirstArraySlice = 0;
        HR(g_pd3dDevice->CreateRenderTargetView(g_LR_tex.Get(), &rtv_desc, &g_LR_RTV));
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

    // Default wide open viewport
    D3D11_VIEWPORT vp;
    vp.Width    = (FLOAT)g_ScreenWidth;
    vp.Height   = (FLOAT)g_ScreenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Compile the vertex shader
    ID3DBlob* vs_blob = nullptr;
    hr                = compile_shader_from_file(L"Tutorial07.fx", "VS", "vs_4_0", &vs_blob);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &g_pVertexShader);
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
    hr = g_pd3dDevice->CreateInputLayout(layout, num_elements, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &g_pVertexLayout);
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
    hr = g_pd3dDevice->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &g_pPixelShader);
    if (FAILED(hr))
    {
        ps_blob->Release();
        return hr;
    }

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

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
// Render current image, eye independent. eye_cb is this eye's projection setup;
// updated here (not by the caller) because the load pass below also writes b0.
//--------------------------------------------------------------------------------------
void draw_cube(bool rightEye, const shared_CB& eye_cb)
{
    // Clear the eye buffers
    //
    g_pImmediateContext->ClearRenderTargetView(g_LR_RTV.Get(), Colors::OliveDrab);

    // Clear the depth buffer to 1.0 (max depth)
    //
    // Also done on a per-eye basis.
    //
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    assert(g_LR_RTV);  // Must be allocated.

    // Set the RenderTargetView for both RTV slices.
    ID3D11RenderTargetView* rtv_array[] = { g_LR_RTV.Get() };
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

    // Checking for possible fatal errors that could cause an eye swap situation.
    // Does not seem to ever hit exception handler, which is what we'd expect.
    try
    {
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
// Game side of the geo-11 handoff (CopyToHandoff port): publish the just-rendered
// L/R pair into the next FIFO slot under its keyed mutex, with the readiness
// fence signaled behind the copies. Never wedges the game thread: a 150ms cap
// means a hung presenter costs one dropped frame, not a hang.
//--------------------------------------------------------------------------------------
void copy_to_handoff()
{
    ID3D11Resource* bb = NULL;
    HR(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&bb));

    // Copy Left eye into output backbuffer.
    g_pImmediateContext->CopySubresourceRegion(bb, 0, 0, 0, 0, g_LR_tex.Get(), 0, nullptr);
    HR(g_pSwapChain->Present(0, 0));

    // Copy Right eye into output backbuffer. (second slice/subresource)
    g_pImmediateContext->CopySubresourceRegion(bb, 0, 0, 0, 0, g_LR_tex.Get(), 1, nullptr);
    HR(g_pSwapChain->Present(1, 0));
}
