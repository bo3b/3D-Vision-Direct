#pragma once

#include <Windows.h>
#include <d3d11.h>

// Testing parameters

inline UINT g_bufferCount = 4;  // Quad buffered stereo- Front/Back, next up Front/Back

inline UINT g_ScreenWidth  = 2560;  // Starting window size
inline UINT g_ScreenHeight = 1440;

inline bool g_judder_bar = true;  // F7 toggles a test bar to show judder on repeated/dropped frames

inline DXGI_SWAP_EFFECT g_swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // Allows windowed 3D.

inline UINT g_framerate = 1000 / (20);  // 50Hz for testing judder, etc. (in ms)