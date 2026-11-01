#ifndef ENGINE_SHADER_CONSTANT_BUFFER_HLSLI
#define ENGINE_SHADER_CONSTANT_BUFFER_HLSLI

cbuffer MaterialBuffer
{
    float4 g_BaseColorTint;
    float4 g_EmissiveColor;
    float g_EmissiveIntensity;
    float g_NormalStrength;
    float g_OpacityMaskClipValue;
    float g_AoScale;
    float g_RoughnessScale;
    float g_MetalnessScale;
    uint g_TextureMask;
    uint g_MaterialFlags;           // MATERIAL_FLAG_*
    float g_Metallic;               // used when MATERIAL_FLAG_METALLIC_OVERRIDE
    float g_Roughness;              // used when MATERIAL_FLAG_ROUGHNESS_OVERRIDE (= 1 - NieR g_Glossiness)
    float2 g_DetailNormalTile;
    float g_DetailNormalWeight;
    float g_Intensity;
    float2 g_UVTile;                // preset uvTile: texcoord multiplier (NieR g_Tile, PBS10 g_MaterialTile1)
    float2 g_UVTile2;               // preset uvTile2: second-UV multiplier (PBS10 g_MaterialTile2)
    float2 g_MaterialPadding;
};

cbuffer CameraBuffer
{
    row_major float4x4 g_ViewMatrix;
    row_major float4x4 g_ProjMatrix;
    row_major float4x4 g_ViewInverseMatrix;
    row_major float4x4 g_ProjInverseMatrix;
    float4 g_CameraPosition;
    float4 g_Time;          // x seconds the main timer has run (time scale applied)
};

cbuffer ObjectBuffer
{
    row_major float4x4 g_WorldMatrix;
};

cbuffer LightBuffer
{
    float4 g_LightDir;
    float4 g_LightPosition;
    float4 g_LightDiffuse;
    float4 g_LightAmbient;
    float4 g_LightSpecular;
    float g_LightRange;
    float3 g_LightSpot;     // spot: cos outer cone, cos inner cone, beam scattering (1/m; 0 = none)
};

cbuffer ScreenBuffer
{
    row_major float4x4 g_ScreenProjMatrix;
    float4 g_ScreenTexel;   // 1/w, 1/h, w, h
    float4 g_SSAOParams;    // radius, intensity, bias, enabled
};

// Screen post-processing (PostProcessCB, PostProcessShader). The sun comes from LightBuffer.
cbuffer PostProcessBuffer
{
    float4 g_ColorGrade;        // exposure, contrast, saturation, bloom intensity
    float4 g_ColorTint;         // grading tint (multiply)
    float4 g_Volumetric;        // sun scattering intensity, air density (1/m), march distance (m), enabled
    float4 g_VolumetricPhase;   // x Henyey-Greenstein g
    float4 g_Fog;               // level fog color (rgb), max opacity (w; 0 = off)
    float4 g_FogParams;         // x fog distance (m): the haze reaches 63% of the max opacity there, yzw sun glow color
    float4 g_Glow;              // x 1 = RT_EMISSIVE was drawn this view, y how strongly the glow blooms, z glitch colour split strength, w glitch band slip strength
};

// Directional light shadow (ShadowCB): baked static map + SHADOW_CASCADE_COUNT = 2 dynamic cascades (Engine_Rendering_Const.h).
cbuffer ShadowBuffer
{
    row_major float4x4 g_ShadowMatrices[2];    // world -> dynamic cascade UV (xy) and light depth (z)
    row_major float4x4 g_BakedShadowMatrix;    // world -> baked static map UV (xy) and light depth (z)
    float4 g_ShadowSplits;                     // view depth where each dynamic cascade ends (x, y)
    float4 g_ShadowParams;                     // x strength, y depth bias, z 1 / map size, w blend share
    float4 g_BakedShadowParams;                // x 1 = baked map valid, y depth bias, z 1 / map size, w texel world size
    float4 g_ShadowDebug;                      // x 1 = tint the lit scene by cascade (F9)
};

// Skinning palette, MODEL_BONE_MAX = 512 (Engine_ModelStruct.h). 32KB of the 64KB cbuffer limit.
cbuffer SkeletalBuffer
{
    row_major float4x4 g_BoneMatrices[512];
};

#endif
