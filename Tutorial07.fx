//--------------------------------------------------------------------------------------
// File: Tutorial07.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------

cbuffer cbShared : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;
	uint   EyeIndex;   // Selects the g_LR_RTV array slice: 0 = left, 1 = right.
    uint   Load;       // Load count (0, 8000, 16000, 32000, 64000) for the GPU loading PS.
	uint2  pad;        // Constant buffers must be a multiple of 16 bytes.
};


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul( input.Pos, World );
    output.Pos = mul( output.Pos, View );
    output.Pos = mul( output.Pos, Projection );
    output.Tex = input.Tex;

    return output;
}


//--------------------------------------------------------------------------------------
// Geometry Shader: pure passthrough, only exists to stamp
// SV_RenderTargetArrayIndex, which only a GS (or DS) can output on shader
// model 4. Routes the whole draw call to the eye's slice of g_LR_RTV, so the
// two sequential per-eye draws in draw_cube land in slice 0 and slice 1
// instead of both landing in slice 0.
//--------------------------------------------------------------------------------------
struct GS_OUTPUT
{
    float4 Pos     : SV_POSITION;
    float2 Tex     : TEXCOORD0;
    uint   RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
void GS( triangle PS_INPUT input[3], inout TriangleStream<GS_OUTPUT> output )
{
    GS_OUTPUT o;
    o.RTIndex = EyeIndex;

    [unroll]
    for (int i = 0; i < 3; i++)
    {
        o.Pos = input[i].Pos;
        o.Tex = input[i].Tex;
        output.Append(o);
    }
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    return float4(0.75, 0.75, 0.75, 1); // Dark grey
}



//--------------------------------------------------------------------------------------
// GPU load Pixel Shader (F8): a long dependent-FMA chain per pixel, drawn on a
// screen-covering cube to simulate a heavy game's multi-ms command-buffer burst
// (the Witcher3-at-max case: ~25ms of GPU work per frame). The chain is serially
// dependent so the compiler cannot vectorize or eliminate it, and the result
// feeds the output (scaled to invisibility) so it cannot be dead-coded.
//--------------------------------------------------------------------------------------

float4 PS_Load(PS_INPUT input) : SV_Target
{
    float acc = input.Tex.x;
    [loop]
    for (uint i = 0; i < Load; i++)
        acc = acc * 1.0000001f + 0.0000001f;
    return float4(0.25f + acc * 1e-30f, 0.25f, 0.25f, 1);
}

