#pragma once

#include <Windows.h>

// The stereo rendering for the display. We want this standalone so that
// we can modify it more easily than being junked up with all the init stuff.
//
// This is just the basic drawing of the cube in stereo.

HRESULT init_dx11(HWND window);
void    cleanup_device();
void    render_frame();
void    copy_to_handoff();
void    fullscreen(bool set);