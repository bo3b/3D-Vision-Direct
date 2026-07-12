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
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    return float4(0.5, 0.5, 0.5, 1);
}


//--------------------------------------------------------------------------------------
// GPU load Pixel Shader (F8): a long dependent-FMA chain per pixel, drawn on a
// screen-covering cube to simulate a heavy game's multi-ms command-buffer burst
// (the Witcher3-at-max case: ~25ms of GPU work per frame). The chain is serially
// dependent so the compiler cannot vectorize or eliminate it, and the result
// feeds the output (scaled to invisibility) so it cannot be dead-coded.
//--------------------------------------------------------------------------------------
cbuffer cbLoad : register( b1 )
{
	uint4 LoadParams;   // x = FMA iterations per pixel.
};

float4 PS_Load( PS_INPUT input ) : SV_Target
{
    float acc = input.Tex.x;
    [loop] for (uint i = 0; i < LoadParams.x; i++)
        acc = acc * 1.0000001f + 0.0000001f;
    return float4(0.25f + acc * 1e-30f, 0.25f, 0.25f, 1);
}
