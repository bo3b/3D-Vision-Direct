#pragma once

#include "Display.h"
#include "Timer.h"
#include "ShutterGlasses.h"

#include <Windows.h>
#include <wrl/client.h>
#include <d3d11_1.h>

using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Testing parameters
//--------------------------------------------------------------------------------------

inline UINT g_bufferCount = 3;  // Quad buffered stereo- Front/Back, next up Front/Back

inline UINT g_ScreenWidth  = 2560;  // Starting window size
inline UINT g_ScreenHeight = 1440;

inline bool g_judder_bar = true;  // F7 toggles a test bar to show judder on repeated/dropped frames

inline DXGI_SWAP_EFFECT g_swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

inline UINT frame_rate  = 20;
inline UINT g_SleepTime = 1000 / (frame_rate);  // 50Hz for testing judder, etc. (in ms)

//--------------------------------------------------------------------------------------
// App/Game globals
//--------------------------------------------------------------------------------------
inline HINSTANCE g_hInst = nullptr;  // 'game' instance
inline HWND      g_hWnd  = nullptr;  // Main window created by the 'game'

inline Display* g_Display = nullptr;  // output display

inline Timer g_Timer;  // microsecond accuracy

//--------------------------------------------------------------------------------------
// StereoRender globals
//--------------------------------------------------------------------------------------
inline IDXGISwapChain*      g_GameSwapChain        = nullptr;
inline ID3D11Device*        g_GameDevice           = nullptr;
inline ID3D11DeviceContext* g_GameImmediateContext = nullptr;

// Per-eye render targets on the game device (not shared - the handoff below is
// the only cross-device surface, matching geo-11's fake-backbuffer -> handoff shape).
extern ComPtr<ID3D11Texture2D>        g_LR_tex;  // ArraySize=2
extern ComPtr<ID3D11RenderTargetView> g_LR_RTV;  // Requires special VS for slices

// Most recent two layer frame finished by the game.
// Stored for pickup by the monitor presenter.
inline ComPtr<ID3D11Texture2D> g_game_latest_LR;  // ArraySize=2

//--------------------------------------------------------------------------------------
// Display output globals
//--------------------------------------------------------------------------------------
inline NvidiaShutterGlasses g_shutterGlasses;
inline UINT                 g_FrameLatency = 2;