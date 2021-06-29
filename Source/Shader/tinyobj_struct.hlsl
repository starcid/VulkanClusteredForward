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
struct TransformData
{
    matrix mvp;
    matrix model;
    matrix view;
    matrix proj;
    matrix proj_view;
    float3 cam_pos;
    bool isClusteShading;
    uint4 tileSizes;
    float zNear;
    float zFar;
    float scale;
    float bias;
    vector light_pos[MAX_LIGHT_NUM];
};

struct PointLightData
{
    float3 pos;
    float radius;
    float3 color;
    uint enabled;
    float ambient_intensity;
    float diffuse_intensity;
    float specular_intensity;
    float attenuation_constant;
    float attenuation_linear;
    float attenuation_exp;
    float2 padding;
};

struct MaterialData
{
    int has_albedo_map;
    int has_normal_map;
};

struct LightGrid 
{
    uint offset;
    uint count;
};

struct PSInput
{
    vector position : SV_POSITION;
    float3 fragColor : COLOR;
    float3 fragTexCoord : TEXCOORD;
    float3 fragPos : POSITION0;
    float3 tanViewPos : POSITION1;
    float3 tanLightPos[MAX_LIGHT_NUM] : POSITION2;
};
