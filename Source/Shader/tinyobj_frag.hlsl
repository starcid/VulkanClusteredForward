//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#define MAX_LIGHT_NUM 16
cbuffer TransformData : register(b0)
{
    mat4 mvp;
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 proj_view;
    vec3 cam_pos;
    bool isClusteShading;
    uvec4 tileSizes;
    float zNear;
    float zFar;
    float scale;
    float bias;
    vec4 light_pos[MAX_LIGHT_NUM];
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = position + offset;
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
