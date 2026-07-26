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

inline UINT g_bufferCount = 5;  // Quad buffered stereo- Front/Back, next up Front/Back

inline UINT g_ScreenWidth  = 2560;  // Starting window size
inline UINT g_ScreenHeight = 1440;

inline bool g_judder_bar = true;  // F7 toggles a test bar to show judder on repeated/dropped frames

inline DXGI_SWAP_EFFECT g_swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

// Load parameters for the artificially long Draw call. This redraws in the PS_Load the
// g_stall_iterations time, so that we don't return from Draw for a long time. Can degenerate
// into too much work to ever be Presented in the 8.3ms frame timing.
inline UINT       g_stall_iterations;  // Also shown in ImGUI
inline UINT       load_index = 5;      // Start at
inline const UINT g_loads[]  = { 0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000 };

// Load parameters for the main drawing of N cubes.  We can add 5 more cubes at each F6,
// or then also add more PS_Load waste per cube with F5.
inline UINT       g_cube_iterations;                                      // FMA iterations per pixel for EACH scene cube's PS.
static int        cube_load_index = 1;                                    // 
inline const UINT g_cube_loads[]  = { 0, 50, 100, 200, 400, 800, 1600 };  // F5 cycles.
inline UINT       g_cube_rows     = 1;                                    // Cube grid depth. 5 cubes wide × g_cube_rows deep.
                                                                          // F6 adds another row going into the screen; a game-like
                                                                          // spread of draw calls, distinct from PS_Load's single-shader spike.

//--------------------------------------------------------------------------------------
// App/Game globals
//--------------------------------------------------------------------------------------
inline HINSTANCE g_hInst = nullptr;  // 'game' instance
inline HWND      g_hWnd  = nullptr;  // Main window created by the 'game'

inline Display* g_Display = nullptr;  // output display

inline Timer     g_Timer;      // microsecond accuracy
inline GpuTimer* g_LoadTimer;  // ms accuracy?

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

inline UINT g_DroppedFrames = 0;
