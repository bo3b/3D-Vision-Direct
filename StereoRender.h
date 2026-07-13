#pragma once

// The stereo rendering for the display. We want this standalone so that
// we can modify it more easily than being junked up with all the init stuff.
//
// This is just the basic drawing of the cube in stereo.

#include <wrl/client.h>
#include <d3d11.h>
#include <Windows.h>

using Microsoft::WRL::ComPtr;

// Per-eye render targets on the game device (not shared - the handoff below is
// the only cross-device surface, matching geo-11's fake-backbuffer -> handoff shape).
extern ComPtr<ID3D11Texture2D>        g_LR_tex;  // ArraySize=2
extern ComPtr<ID3D11RenderTargetView> g_LR_RTV;  // Requires special VS for slices

HRESULT init_dx11(HWND window);
void    cleanup_device();
void    render_frame();
void    copy_to_handoff();
void	fullscreen(bool set);