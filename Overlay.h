#pragma once

#include <Windows.h>
#include <d3d11_1.h>

class Overlay
{
private:
    ID3D11DeviceContext*    context_   = nullptr;  // game immediate context (not owned)
    ID3D11RenderTargetView* eyeRTV_[2] = {};       // single-slice RTVs, one per eye of the scene array

public:
    Overlay(HWND game_window, ID3D11Device* game_device, ID3D11DeviceContext* game_immediate_context, ID3D11Texture2D* scene_LR);
    ~Overlay();

    void Render();
};
