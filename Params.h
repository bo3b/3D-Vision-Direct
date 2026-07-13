#pragma once

#include <Windows.h>

// Testing parameters

inline UINT g_bufferCount = 4;  // Quad buffered stereo- Front/Back, next up Front/Back

inline UINT g_ScreenWidth  = 1920;  // Starting window size
inline UINT g_ScreenHeight = 1080;

inline bool g_judder_bar = false;  // F7 toggles a test bar to show judder on repeated/dropped frames