#pragma once

#include <Windows.h>
#include <d3d11_1.h>

class Overlay
{
private:
    ID3D11DeviceContext*    context_   = nullptr;  // game immediate context (not owned)
    ID3D11RenderTargetView* eyeRTV_[2] = {};       // single-slice RTVs, one per eye of the scene array

    // Rolling history of raw per-frame FPS (1/DeltaTime, not io.Framerate which is
    // pre-smoothed). PlotLines walks this as a ring starting from fps_history_idx_,
    // so oldest sits on the left of the graph and newest on the right.
    static constexpr int kFpsHistorySize                = 240;
    float                fps_history_[kFpsHistorySize]  = {};
    int                  fps_history_idx_               = 0;
    float                load_history_[kFpsHistorySize] = {};
    int                  load_history_idx_              = 0;

public:
    Overlay(HWND game_window, ID3D11Device* game_device, ID3D11DeviceContext* game_immediate_context, ID3D11Texture2D* scene_LR);
    ~Overlay();

    void Render();
};
