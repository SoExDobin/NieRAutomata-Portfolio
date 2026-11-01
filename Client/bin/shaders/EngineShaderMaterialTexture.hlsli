#ifndef ENGINE_SHADER_MATERIAL_TEXTURE_HLSLI
#define ENGINE_SHADER_MATERIAL_TEXTURE_HLSLI

// Material texture slots of model shaders (Shader_VtxMesh, Shader_VtxAnimMesh, Shader_VtxPlayer).
// Names match Material slot names and pl0000.json/wp3000.json "textures" keys; Material binds them by name.
// Textures are SRVs (t registers), so they live here instead of a cbuffer.
// DeferredShader does not include this file: it samples render targets, not material textures.

#include "EngineShaderDefine.hlsli"     // LinearWrapSampler
#include "EngineShaderMaterial.hlsli"   // g_TextureMask, Node_NormalMap

Texture2D g_AlbedoMap;
Texture2D g_NormalMap;
Texture2D g_MaskMap;
// NieR g_DetailNormalMap (player W_PBS/W_Skin, weapons, map PBS00). Optional: test with Node_IsBound.
Texture2D g_DetailNormalMap;
// Glow mask (R): a machine's eyes, authored per model; only the EM_Eye pass reads it (MATERIAL_TEXTURE_EMISSIVE).
Texture2D g_EmissiveMap;

// Blender Image Texture(g_AlbedoMap) -> Base Color, with the preset tint and emissive added.
// Alpha stays the texture alpha so the pass can run Node_OpacityClip on it.
float4 Node_MaterialBaseColor(float2 uv)
{
    float4 color = g_AlbedoMap.Sample(LinearWrapSampler, uv);
    if (g_MaterialFlags & MATERIAL_FLAG_UNPREMULTIPLY_ALPHA)
        color = Node_UnpremultiplyAlpha(color);
    color *= g_BaseColorTint;
    color.rgb += g_EmissiveColor.rgb * g_EmissiveIntensity;
    return color;
}

// ShadowCompatible pixel stage: depth only, no colour output. Cuts the same alpha holes the colour pass
// cuts (the material's opacityMaskClipValue), so foliage and hair cast their cut-out shapes.
void Node_ShadowDepthClip(float2 uv)
{
    Node_OpacityClip(Node_MaterialBaseColor(uv).a);
}

// Blender Image Texture(g_MaskMap). RGB is decoded as sRGB when the preset sets maskSrgb (the .blend
// image color space); alpha is never color managed.
float4 Node_MaterialMask(float2 uv)
{
    float4 mask = g_MaskMap.Sample(LinearWrapSampler, uv);
    if (g_MaterialFlags & MATERIAL_FLAG_MASK_SRGB)
        mask.rgb = Node_SrgbToLinear(mask.rgb);
    return mask;
}

// g_MaskMap, or MATERIAL_DEFAULT_MASK when the material binds none.
float4 Node_MaterialMaskOrDefault(float2 uv)
{
    if (g_TextureMask & MATERIAL_TEXTURE_ORM)
        return Node_MaterialMask(uv);
    return MATERIAL_DEFAULT_MASK;
}

// Texcoord the PBS00 graph samples with: "If Non-Zero(g_UV2Use)" picks UV2 (preset useUV2) or UV1.
float2 Node_MaterialUV(float2 uv1, float2 uv2)
{
    return (g_MaterialFlags & MATERIAL_FLAG_UV2) ? uv2 : uv1;
}

// Blender Image Texture(g_NormalMap) -> Normal Map -> BSDF Normal.
// Falls back to the vertex normal when the material has no normal map (MATERIAL_TEXTURE_NORMAL off).
float3 Node_MaterialNormal(float2 uv, float3 worldNormal, float3 worldTangent)
{
    if (0 == (g_TextureMask & MATERIAL_TEXTURE_NORMAL))
        return normalize(worldNormal);
    return Node_NormalMap(g_NormalMap.Sample(LinearWrapSampler, uv).rgb, worldNormal, worldTangent);
}

// PBS00 detail normal: DetailNormalMap(uv x g_DetailNormalTile) -> Normal Map, scaled by weight
// (MaskMap.A) x g_DetailNormalWeight and added to the base normal. Unchanged when no detail map is bound.
float3 Node_AddDetailNormal(float3 normal, float2 uv, float weight, float3 worldNormal, float3 worldTangent)
{
    if (!Node_IsBound(g_DetailNormalMap))
        return normal;
    const float3 detail = Node_NormalMap(
        g_DetailNormalMap.Sample(LinearWrapSampler, uv * g_DetailNormalTile).rgb, worldNormal, worldTangent);
    return normalize(detail * weight * g_DetailNormalWeight + normal);
}

#endif
