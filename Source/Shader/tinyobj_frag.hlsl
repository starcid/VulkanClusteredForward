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

cbuffer Material : register(b1)
{
    MaterialData material;
};

cbuffer PointLights : register(b2)
{
    PointLightData pointLight[MAX_LIGHT_NUM];
}

RWBuffer<uint> globalLightIndexList : register(u1);
RWBuffer<LightGrid> lightGrid: register(u2);

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);

SamplerState albedoSampler : register(s0);
SamplerState normalSampler : register(s1);

float linearDepth(float depthSample) 
{
    float depthRange = 2.0 * depthSample - 1.0;
    float linearv = 2.0 * transform.zNear * transform.zFar / (transform.zFar + transform.zNear - depthRange * (transform.zFar - transform.zNear));
    return linearv;
}

float3 lightingColor(uint i, PSInput IN);

float4 PSMain(PSInput IN) : SV_TARGET
{
    float4 outColor = float4(0,0,0,1);

    if (transform.isClusteShading)
    {
        uint zTile = uint(max(log2(linearDepth(IN.position.z)) * transform.scale + transform.bias, 0.0));
        uint3 tiles = uint3(uint2((IN.position.xy / transform.tileSizes[3]).xy), zTile);
        uint tileIndex = tiles.x +
                        transform.tileSizes.x * tiles.y +
                        (transform.tileSizes.x * transform.tileSizes.y) * tiles.z;

        uint offset = lightGrid[tileIndex].offset;
        uint visibleLightCount = lightGrid[tileIndex].count;
        [unroll(MAX_LIGHT_NUM)] for (uint idx = 0; idx < visibleLightCount; idx++)
        {
            uint i = globalLightIndexList[offset + idx];

            // final color
            outColor.xyz += lightingColor(i, IN);
        }
    }
    else
    {
        for (int i = 0; i < MAX_LIGHT_NUM; i++)
        {
            // final color
            outColor.xyz += lightingColor(i, IN);
        }
    }
    return outColor;
}

float3 lightingColor(uint i, PSInput IN)
{
    // diffuse
    float3 albedo;
    if (material.has_albedo_map > 0)
    {
        albedo = albedoTex.Sample(albedoSampler, IN.fragTexCoord.xy).rgb;
    }
    else
    {
        albedo = float3(1.0, 1.0, 1.0);
    }
    // ambient
    float3 ambient = pointLight[i].ambient_intensity * albedo * pointLight[i].color;
    // normal
    float3 normal;
    if (material.has_normal_map > 0)
    {
        normal = normalTex.Sample(normalSampler, IN.fragTexCoord.xy).rgb;
        normal = normalize(normal * 2.0 - 1.0);
    }
    else
    {
        normal = float3(0, 0, 1);
    }
    // diffuse
    float3 lightDir = normalize(IN.tanLightPos[i]);
    float lambertian = max(dot(lightDir, normal), 0.0);
    float3 diffuse = pointLight[i].diffuse_intensity * albedo * lambertian * pointLight[i].color;
    // specular
    float3 viewDir = normalize(IN.tanViewPos);
    float3 halfDir = normalize(lightDir + viewDir);
    float specAngle = max(dot(halfDir, normal), 0.0);
    float spec = pow(specAngle, 32);
    float3 specular = pointLight[i].specular_intensity * spec * pointLight[i].color;
    // attenuation
    float distance = length(pointLight[i].pos - IN.fragPos);
    float attenuation = pow(smoothstep(pointLight[i].radius, 0, distance), 2);
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    // final color
    return float3(ambient + diffuse + specular);
}
