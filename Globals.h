#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <d3d11.h>

using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Testing parameters
//--------------------------------------------------------------------------------------

inline UINT g_bufferCount = 4;  // Quad buffered stereo- Front/Back, next up Front/Back

inline UINT g_ScreenWidth  = 3840;  // Starting window size
inline UINT g_ScreenHeight = 2160;

inline bool g_judder_bar = true;  // F7 toggles a test bar to show judder on repeated/dropped frames

inline DXGI_SWAP_EFFECT g_swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

inline UINT g_framerate = 1000 / (20);  // 50Hz for testing judder, etc. (in ms)

//--------------------------------------------------------------------------------------
// App/Game globals
//--------------------------------------------------------------------------------------
inline HINSTANCE g_hInst = nullptr;  // 'game' instance
inline HWND      g_hWnd  = nullptr;  // Main window created by the 'game'

//--------------------------------------------------------------------------------------
// StereoRender globals
//--------------------------------------------------------------------------------------
// Per-eye render targets on the game device (not shared - the handoff below is
// the only cross-device surface, matching geo-11's fake-backbuffer -> handoff shape).
extern ComPtr<ID3D11Texture2D>        g_LR_tex;  // ArraySize=2
extern ComPtr<ID3D11RenderTargetView> g_LR_RTV;  // Requires special VS for slices

// Most recent two layer frame finished by the game.
// Stored for pickup by the monitor presenter.
extern ComPtr<ID3D11Texture2D> g_Latest_LR;  // ArraySize=2
