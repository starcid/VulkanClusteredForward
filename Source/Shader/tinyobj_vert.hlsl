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
#include "tinyobj_struct.hlsl"

cbuffer Transform : register(b0)
{
    TransformData transform;
}

cbuffer PointLights : register(b1)
{
    PointLightData pointLight[MAX_LIGHT_NUM];
}

PSInput VSMain(float4 inPosition : POSITION, float4 inColor : COLOR, float4 inTexcoord : TEXCOORD, float4 inNormal : NORMAL, float4 inTangent : TANGENT)
{
    PSInput OUT;

    OUT.position = mul(transform.mvp, inPosition);
    OUT.fragColor = inColor.xyz;
    OUT.fragTexCoord = inTexcoord.xyz;
    OUT.fragPos = mul(transform.model, inPosition).xyz;

    float3 bitangent = cross(inNormal.xyz, inTangent.xyz);
    float3 v = transform.cam_pos - inPosition.xyz;
    OUT.tanViewPos = float3(dot(inTangent.xyz, v), dot(bitangent, v), dot(inNormal.xyz, v));
    for (int i = 0; i < MAX_LIGHT_NUM; i++)
    {
        float3 l = (transform.light_pos[i] - inPosition).xyz;
        OUT.tanLightPos[i] = float3(dot(inTangent.xyz, l), dot(bitangent, l), dot(inNormal.xyz, l));
    }

    return OUT;
}

