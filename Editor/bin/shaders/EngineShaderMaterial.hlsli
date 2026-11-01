#ifndef ENGINE_SHADER_MATERIAL_HLSLI
#define ENGINE_SHADER_MATERIAL_HLSLI

// Shading nodes shared by the Client G-buffer shaders and DeferredShader.
// Each node mirrors one step of the NieR2Blender material graph, so a shader reads like
// the Blender node chain it reproduces. Material values come from MaterialBuffer.

#include "EngineShaderConstantBuffer.hlsli"
#include "EngineShaderDefine.hlsli"     // ShadowSampler

// Same bits as MATERIAL_TEXTURE_MASK in Shader_CBuffer_Define.h; Material sets them for the slots it binds.
#define MATERIAL_TEXTURE_NORMAL (1u << 1)
#define MATERIAL_TEXTURE_ORM (1u << 2)
#define MATERIAL_TEXTURE_EMISSIVE (1u << 3)

// Same bits as MATERIAL_FLAG in Shader_CBuffer_Define.h; set from the material settings "parameters".
#define MATERIAL_FLAG_METALLIC_OVERRIDE (1u << 0)
#define MATERIAL_FLAG_ROUGHNESS_OVERRIDE (1u << 1)
#define MATERIAL_FLAG_MASK_SRGB (1u << 2)
#define MATERIAL_FLAG_UV2 (1u << 3)
#define MATERIAL_FLAG_UNPREMULTIPLY_ALPHA (1u << 4)

// NieR mask when a material has none bound (the game's white default texture is not extracted):
// R metal 0, G gloss 0 (roughness 1 after Invert Green), B cavity 1, A detail weight 0.
#define MATERIAL_DEFAULT_MASK float4(0.f, 0.f, 1.f, 0.f)

// Blender Image Texture with color space "sRGB": the texel is decoded to linear before any node uses it.
float3 Node_SrgbToLinear(float3 color)
{
    const float3 low = color / 12.92f;
    const float3 high = pow((color + 0.055f) / 1.055f, 2.4f);
    return lerp(high, low, step(color, 0.04045f));
}

// Inverse alpha for DDS cutouts. BC1 (DXT1) stores every transparent texel as RGB 0 + alpha 0, and NieR's
// DXT5 cutouts also fill their empty space with black, where a PNG would keep the color and let only the
// alpha mark the hole. Filtering then mixes that black into the edge texels (dark fringes), so the color is
// divided by the filtered alpha to restore it; the pass's alpha test removes the empty texels.
// Only for albedos whose alpha is coverage - PBS10 stores a height in it (preset unpremultiplyAlpha).
float4 Node_UnpremultiplyAlpha(float4 color)
{
    if (color.a > 0.0001f)
        color.rgb = saturate(color.rgb / color.a);
    return color;
}

// Blender Color Attribute node: byte vertex colors are stored sRGB (FBX export default) and read linear.
float4 Node_VertexColor(float4 color)
{
    return float4(Node_SrgbToLinear(color.rgb), color.a);
}

#define SHADING_PI 3.14159265f

struct SurfaceData
{
    float4 albedo;
    float3 normal;      // world space, unit length
    float metalness;
    float roughness;
    float ao;
    float depth;        // SV_POSITION.z of the pixel (clip z / w)
};

// G-buffer layout of MRT_GameObject: RT_DIFFUSE, RT_NORMAL, RT_MATERIAL, RT_DEPTH.
// Every shader that draws into NONBLEND must write all four, or the previous object's
// material and depth stay behind in the targets it skipped.
struct PS_GBUFFER_OUT
{
    float4 diffuse : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
    float depth : SV_TARGET3;
};

// ---- Surface nodes (G-buffer write) ----

// Blender "Invert Green" group: NieR stores gloss in G, the BSDF wants roughness.
float3 Node_InvertGreen(float3 color)
{
    return float3(color.r, 1.f - color.g, color.b);
}

// Normal map convention of the assets. NieR normal maps are DirectX style (G points down); Blender
// flips G with Invert Green because it expects OpenGL, D3D reads them as they are.
// Define it as -1.f before including this file if the bumps look inverted.
#ifndef NORMAL_MAP_GREEN_SIGN
#define NORMAL_MAP_GREEN_SIGN 1.f
#endif

// Whether a texture slot is bound. D3D11 reports 0 x 0 for a null SRV.
// Use it for slots without a MATERIAL_TEXTURE_* bit (detail normal, curvature, ...).
bool Node_IsBound(Texture2D map)
{
    uint width, height;
    map.GetDimensions(width, height);
    return width > 0;
}

// Blender "Binarize (Float)" group: 1 when value > 0.5. NieR g_1BitMask alpha test.
float Node_Binarize(float value)
{
    return value > 0.5f ? 1.f : 0.f;
}

// Normal map texel (0..1) -> tangent-space vector. g_NormalStrength is the Blender Normal Map "Strength".
float3 Node_DecodeNormal(float3 color)
{
    float3 tangentNormal = color * 2.f - 1.f;
    tangentNormal.y *= NORMAL_MAP_GREEN_SIGN;
    tangentNormal.xy *= g_NormalStrength;
    return tangentNormal;
}

// NieR height blend as the .blend builds it: Math(MULTIPLY_ADD: height x -1 + weight) -> Math(DIVIDE 0.001)
// -> Mix Fac (clamped). The next layer shows where the painted vertex weight exceeds the layer's height (alpha).
float Node_HeightBlendFactor(float vertexWeight, float layerHeight)
{
    return saturate((vertexWeight - layerHeight) / 0.001f);
}

// Blender Mix(MULTIPLY) of the mask cavity (B) into the base color; preset aoScale is the Fac.
void Node_MultiplyCavity(inout float3 albedo, float cavity)
{
    albedo *= lerp(1.f, cavity, g_AoScale);
}

// Alpha test with the preset clip value. 0 keeps every pixel; NieR g_1BitMask materials use 0.5.
void Node_OpacityClip(float alpha)
{
    clip(alpha - g_OpacityMaskClipValue);
}

// Tangent space -> world space. The tangent is re-orthogonalised against the normal (Gram-Schmidt).
float3 Node_TangentToWorld(float3 tangentNormal, float3 worldNormal, float3 worldTangent)
{
    const float3 N = normalize(worldNormal);
    const float3 T = normalize(worldTangent - N * dot(worldTangent, N));
    const float3 B = cross(N, T);
    return normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
}

// Two-sided cards (hair): turn the vertex normal toward the camera, as Blender shows backfacing
// surfaces. NieR hair's camera-facing triangles rasterize as back faces, so an SV_IsFrontFace flip
// points them away from the camera (verified 2026-09-27); the view direction does not depend on winding.
float3 Node_TwoSidedNormal(float3 worldNormal)
{
    const float3 cameraForward = float3(g_ViewMatrix._13, g_ViewMatrix._23, g_ViewMatrix._33);
    return dot(worldNormal, cameraForward) > 0.f ? -worldNormal : worldNormal;
}

// Two-sided surfaces with consistent winding (backface culling off in the .blend): back faces use the
// flipped vertex normal. Hair cards need Node_TwoSidedNormal instead (their winding faces away).
float3 Node_FrontFaceNormal(float3 worldNormal, bool isFrontFace)
{
    return isFrontFace ? worldNormal : -worldNormal;
}

// Blender "Normal Map" node (Tangent Space): normal map texel -> world normal.
float3 Node_NormalMap(float3 color, float3 worldNormal, float3 worldTangent)
{
    return Node_TangentToWorld(Node_DecodeNormal(color), worldNormal, worldTangent);
}

// A surface with no mask texture: non-metal, fully rough, no occlusion.
SurfaceData Node_DefaultSurface(float4 albedo, float3 worldNormal, float depth)
{
    SurfaceData surface;
    surface.albedo = albedo;
    surface.normal = normalize(worldNormal);
    surface.metalness = 0.f;
    surface.roughness = 1.f;
    surface.ao = 1.f;
    surface.depth = depth;
    return surface;
}

// Blender: g_MaskMap -> Invert Green -> Separate Color.
// R -> Metallic, G -> Roughness, B -> multiplied into Base Color (cavity / AO).
// Without a bound mask the surface keeps its defaults. aoScale is a strength, so 0 turns the cavity off.
// Metalness/roughness scales and preset overrides are applied afterwards by Node_ApplyMaterialParameters.
void Node_ApplyNierMask(inout SurfaceData surface, float3 mask)
{
    if (g_TextureMask & MATERIAL_TEXTURE_ORM)
    {
        const float3 separated = Node_InvertGreen(mask);
        surface.metalness = separated.r;
        surface.roughness = separated.g;
        surface.ao = saturate(lerp(1.f, separated.b, g_AoScale));
    }
}

// Blender: Invert Green -> Separate Color R -> Metallic, G -> Roughness (separated = Node_InvertGreen(mask)).
void Node_ApplyMetalRoughness(inout SurfaceData surface, float3 separated)
{
    surface.metalness = separated.r;
    surface.roughness = separated.g;
}

// Material preset values on top of whatever the pass derived from its textures:
// metalness/roughness scales, then the overrides (NieR g_bMetalicOverWrite / g_bGlossinessOverWrite),
// which win as they do in the game shader. Every G-buffer pass calls it right before Node_WriteGBuffer.
void Node_ApplyMaterialParameters(inout SurfaceData surface)
{
    surface.metalness = saturate(surface.metalness * g_MetalnessScale);
    surface.roughness = saturate(surface.roughness * g_RoughnessScale);
    if (g_MaterialFlags & MATERIAL_FLAG_METALLIC_OVERRIDE)
        surface.metalness = saturate(g_Metallic);
    if (g_MaterialFlags & MATERIAL_FLAG_ROUGHNESS_OVERRIDE)
        surface.roughness = saturate(g_Roughness);
}

PS_GBUFFER_OUT Node_WriteGBuffer(SurfaceData surface)
{
    PS_GBUFFER_OUT output;
    output.diffuse = surface.albedo;
    output.normal = float4(surface.normal * 0.5f + 0.5f, 1.f);
    output.material = float4(surface.metalness, surface.roughness, surface.ao, 1.f);
    output.depth = surface.depth;
    return output;
}

// Decal passes (BS_GBufferAlphaBlend): albedo, normal and material blend over the surface below by the
// albedo alpha. The blend state keeps the destination alpha (RT_DIFFUSE alpha marks written geometry)
// and write-masks RT_DEPTH, so lighting still reads the depth of the surface below.
PS_GBUFFER_OUT Node_WriteGBufferAlphaBlend(SurfaceData surface)
{
    PS_GBUFFER_OUT output = Node_WriteGBuffer(surface);
    const float alpha = saturate(surface.albedo.a);
    output.diffuse.a = alpha;
    output.normal.a = alpha;
    output.material.a = alpha;
    return output;
}

// Multiply passes (BS_GBufferMultiply): RT_DIFFUSE is multiplied by lerp(1, color, weight); targets 1-3
// are write-masked by the blend state, so the surface below keeps its normal, material and depth.
PS_GBUFFER_OUT Node_WriteGBufferMultiply(float3 color, float weight)
{
    PS_GBUFFER_OUT output = (PS_GBUFFER_OUT)0;
    output.diffuse = float4(lerp(float3(1.f, 1.f, 1.f), color, saturate(weight)), 1.f);
    return output;
}

// ---- Lighting nodes (deferred pass) ----

// Screen uv + G-buffer depth -> world position, through the camera's inverse matrices.
float3 Node_ReconstructWorldPosition(float2 texcoord, float depth,
    float4x4 projInverse, float4x4 viewInverse)
{
    const float4 clipPosition = float4(texcoord.x * 2.f - 1.f, (1.f - texcoord.y) * 2.f - 1.f, depth, 1.f);
    float4 viewPosition = mul(clipPosition, projInverse);
    viewPosition /= viewPosition.w;
    return mul(viewPosition, viewInverse).xyz;
}

// Cook-Torrance with GGX distribution, Schlick-GGX geometry and Schlick Fresnel.
// Returns the specular radiance for one light, already multiplied by N.L.
float3 Node_SpecularGGX(float3 normal, float3 toLight, float3 toEye,
    float3 albedo, float metalness, float roughness)
{
    const float3 halfVector = normalize(toLight + toEye);
    const float NdotL = saturate(dot(normal, toLight));
    const float NdotV = saturate(dot(normal, toEye));
    const float NdotH = saturate(dot(normal, halfVector));
    const float VdotH = saturate(dot(toEye, halfVector));

    const float alpha = max(roughness * roughness, 0.002f);
    const float alpha2 = alpha * alpha;
    const float denominator = NdotH * NdotH * (alpha2 - 1.f) + 1.f;
    const float distribution = alpha2 / (SHADING_PI * denominator * denominator);

    const float k = (roughness + 1.f) * (roughness + 1.f) / 8.f;
    const float geometry = (NdotL / (NdotL * (1.f - k) + k)) * (NdotV / (NdotV * (1.f - k) + k));

    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalness);
    const float3 fresnel = f0 + (1.f - f0) * pow(1.f - VdotH, 5.f);

    // Back-facing to the light or the eye: no highlight. max() keeps the division finite there.
    const float visible = (NdotL > 0.f && NdotV > 0.f) ? 1.f : 0.f;
    return visible * distribution * geometry * fresnel / max(4.f * NdotL * NdotV, 1e-4f) * NdotL;
}

// Environment reflection from a sky/ground gradient built from the light: sky = ambient + the light
// color (a sunlit sky), ground = ambient + 0.35 light (lit ground bounce). Blender lights metals with its world HDRI; without an environment term a metal
// (hair mask 0.7-1, dress mask R 0.82) keeps only the GGX highlight and reads black.
// The roughness/view falloff is Karis' analytic fit of the split-sum environment BRDF.
float3 Node_EnvironmentSpecular(float3 normal, float3 toEye, float3 albedo, float metalness, float roughness,
    float3 ambient, float3 lightColor)
{
    const float NdotV = saturate(dot(normal, toEye));
    const float3 reflected = reflect(-toEye, normal);
    // Rough surfaces see a blurrier environment, closer to the normal's hemisphere.
    const float up = saturate(lerp(reflected.y, normal.y, roughness) * 0.5f + 0.5f);
    const float3 radiance = lerp(ambient + lightColor * 0.35f, ambient + lightColor, up);
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalness);
    const float4 c0 = float4(-1.f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.f, 0.0425f, 1.04f, -0.04f);
    const float4 r = roughness * c0 + c1;
    const float a004 = min(r.x * r.x, exp2(-9.28f * NdotV)) * r.x + r.y;
    const float2 scaleBias = float2(-1.04f, 1.04f) * a004 + r.zw;
    return radiance * (f0 * scaleBias.x + scaleBias.y);
}

// Final composite: metals have no diffuse, AO darkens both terms like the Blender base color multiply.
float3 Node_CombineLighting(float3 albedo, float metalness, float ao, float3 shade, float3 specular)
{
    return (albedo * (1.f - metalness) * shade + specular) * ao;
}

// Lit HDR color -> the 8-bit screen. Identity up to the knee, then a smooth shoulder toward 1, so a sun brighter
// than 1 burns highlights softly instead of clipping flat; scenes lit within 0..0.8 look the same as before.
float3 Node_ToneMap(float3 color)
{
    const float knee = 0.8f;
    const float3 over = max(color - knee, 0.f);
    return min(color, knee) + (1.f - knee) * (1.f - exp(-over / (1.f - knee)));
}

// ---- Directional light (deferred PS_Directional and the forward BLEND passes) ----

// Dynamic cascades, one slice per cascade (R32F light depth): only moving casters (characters, weapons).
Texture2DArray g_ShadowTexture;
// Static casters (world tiles) baked once per level over its static collision (D16 light depth).
Texture2D g_BakedShadowTexture;

// 3x3 PCF of one cascade. The position is pushed along the normal by 1.5 texels of that cascade first,
// so a surface does not shadow itself where the light grazes it. The depth bias is in metres along the light;
// each cascade's depth range differs, so it is turned into that cascade's depth units here.
float Node_CascadeShadow(uint cascade, float3 worldPosition, float3 normal)
{
    const float4x4 toShadow = g_ShadowMatrices[cascade];
    const float texelWorld = g_ShadowParams.z / length(float3(toShadow._11, toShadow._21, toShadow._31));
    const float3 shadowPosition = mul(float4(worldPosition + normal * texelWorld * 1.5f, 1.f), toShadow).xyz;
    const float compareDepth = shadowPosition.z - g_ShadowParams.y * length(float3(toShadow._13, toShadow._23, toShadow._33));
    float lit = 0.f;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x)
            lit += g_ShadowTexture.SampleCmpLevelZero(ShadowSampler,
                float3(shadowPosition.xy + float2(x, y) * g_ShadowParams.z, cascade), compareDepth);
    return lit / 9.f;
}

// Dynamic cascade covering this view depth: 0..1, or 2 past the dynamic shadow distance.
uint Node_ShadowCascade(float viewDepth)
{
    uint cascade = 0;
    if (viewDepth >= g_ShadowSplits.x) cascade = 1;
    if (viewDepth >= g_ShadowSplits.y) cascade = 2;
    return cascade;
}

// 3x3 PCF of the baked static map, with the same 1.5-texel normal offset. Outside the baked area: lit.
float Node_BakedShadow(float3 worldPosition, float3 normal)
{
    if (g_BakedShadowParams.x <= 0.f)
        return 1.f;
    const float3 shadowPosition = mul(float4(worldPosition + normal * g_BakedShadowParams.w * 1.5f, 1.f), g_BakedShadowMatrix).xyz;
    if (any(shadowPosition.xy < 0.f) || any(shadowPosition.xy > 1.f))
        return 1.f;
    const float compareDepth = shadowPosition.z - g_BakedShadowParams.y;
    float lit = 0.f;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x)
            lit += g_BakedShadowTexture.SampleCmpLevelZero(ShadowSampler,
                shadowPosition.xy + float2(x, y) * g_BakedShadowParams.z, compareDepth);
    return lit / 9.f;
}

// One comparison tap of the baked static map (no PCF, no normal offset): the volumetric march takes many per pixel.
float Node_BakedShadowTap(float3 worldPosition)
{
    const float3 shadowPosition = mul(float4(worldPosition, 1.f), g_BakedShadowMatrix).xyz;
    if (any(shadowPosition.xy < 0.f) || any(shadowPosition.xy > 1.f))
        return 1.f;
    return g_BakedShadowTexture.SampleCmpLevelZero(ShadowSampler, shadowPosition.xy, shadowPosition.z - g_BakedShadowParams.y);
}

// Directional shadow: 1 lit, 0 shadowed. The baked static shadow everywhere in the level, darkened further by
// moving casters near the camera: the dynamic cascade is picked by view depth, the last blend share of each
// cascade fades into the next one, and the last cascade fades back to lit before the dynamic distance ends.
float Node_DirectionalShadow(float3 worldPosition, float3 normal)
{
    if (g_ShadowParams.x <= 0.f)
        return 1.f;
    float shadow = Node_BakedShadow(worldPosition, normal);
    const float viewDepth = mul(float4(worldPosition, 1.f), g_ViewMatrix).z;
    const uint cascade = Node_ShadowCascade(viewDepth);
    if (cascade < 2)
    {
        const float splits[2] = { g_ShadowSplits.x, g_ShadowSplits.y };
        const float cascadeStart = cascade == 0 ? 0.f : splits[0];
        const float blendLength = g_ShadowParams.w * (splits[cascade] - cascadeStart);
        const float blend = saturate((viewDepth - (splits[cascade] - blendLength)) / max(blendLength, 1e-4f));
        float dynamicShadow = Node_CascadeShadow(cascade, worldPosition, normal);
        if (blend > 0.f)
            dynamicShadow = lerp(dynamicShadow, cascade == 0 ? Node_CascadeShadow(1, worldPosition, normal) : 1.f, blend);
        shadow = min(shadow, dynamicShadow);
    }
    return lerp(1.f, shadow, g_ShadowParams.x);
}

// One directional light on one surface: the diffuse light (shade) and the specular for Node_CombineLighting.
// The shadow darkens the direct light only; ambientOcclusion (SSAO, 1 without it) darkens only the ambient
// and the environment reflection. Deferred lighting and the forward passes (water) light a surface the same way.
void Node_DirectionalLight(float3 normal, float3 worldPosition, float3 albedo, float metalness, float roughness,
    float ambientOcclusion, out float3 shade, out float3 specular)
{
    const float3 toLight = -normalize(g_LightDir.xyz);
    const float diffuse = saturate(dot(normal, toLight));
    const float shadow = Node_DirectionalShadow(worldPosition, normal);
    shade = g_LightAmbient.rgb * ambientOcclusion + g_LightDiffuse.rgb * diffuse * shadow;
    const float3 toEye = normalize(g_CameraPosition.xyz - worldPosition);
    specular = Node_SpecularGGX(normal, toLight, toEye, albedo, metalness, roughness) * g_LightSpecular.rgb * shadow;
    // Sky/ground environment reflection: metals (hair, dress) otherwise keep only the GGX highlight and read black.
    specular += Node_EnvironmentSpecular(normal, toEye, albedo, metalness, roughness,
        g_LightAmbient.rgb, g_LightDiffuse.rgb) * ambientOcclusion;
}

#endif
