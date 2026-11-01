#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"
#include "EngineShaderMaterialTexture.hlsli"
#include "EngineShaderVertex.hlsli"

// Map tile shader: the CityOfRuin WorldMap tiles (Models/World/*, main model and _LOD).
// The loader registers "Shader_VtxMesh_Map.hlsl" as VTX_Mesh_Map (VTXMESH layout), WorldMap creates its
// shader with that tag, and the tile presets (ProjectSetting/Material/CityOfRuin*.json, written by the
// Editor stage map-material-presets) name it in "shaderTag". Each pass rebuilds one NieR shader's graph
// as the NieR2Blender .blend builds it. Every material value comes from the preset "parameters";
// shared nodes and slots (g_DetailNormalMap, Node_*) come from the engine hlsli.
// Every .blend map material has backface culling off, so the passes use RS_CullNone and flip the
// normal of back faces (Node_FrontFaceNormal).
// VTXMESH (MODEL_VERSION 3) carries UVMap2 in texcoord1 and the "Col" vertex color in color.

// Map-only slots: PBS10 layers 2 and 3 (NieR g_AlbedoMap2/3, g_NormalMap2/3). Wtr01 uses g_AlbedoMap2
// as its second albedo.
Texture2D g_AlbedoMap2;
Texture2D g_AlbedoMap3;
Texture2D g_NormalMap2;
Texture2D g_NormalMap3;
// MAP_WaterBlend: depth of the opaque scene (RT_DEPTH), bound by Renderer::Bind_ForwardLighting.
Texture2D g_DepthTexture;
// MAP_Water/MAP_WaterBlend: animated ripple normals, the preset texture "g_WaterRippleMap"
// (Texture/Effect/WaterRipple/WaterRipple.dds = coreeff 038.wtp, 30 looping frames of a 64x64 tangent-space normal).
Texture2DArray g_WaterRippleMap;

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float4 color : COLOR;
    float4 clipPosition : TEXCOORD2;    // MAP_WaterBlend: screen position and view depth (w)
    float3 worldPosition : TEXCOORD3;   // MAP_WaterBlend: forward lighting
};

VS_OUT VS_Main(VTXMESH input)
{
    BEGIN_OBJECT_WORLD(input);
    VS_OUT output;
    output.position = Node_ObjectToClip(input.position);
    output.clipPosition = output.position;
    output.worldPosition = mul(float4(input.position, 1.f), OBJECT_WORLD).xyz;
    output.normal = Node_ObjectToWorldDirection(input.normal);
    output.tangent = Node_ObjectToWorldDirection(input.tangent);
    output.texcoord = input.texcoord;
    output.texcoord1 = input.texcoord1;
    output.color = input.color;
    return output;
}

// MAP_PBS00 - PBS00_XXXXX "Default" (418 materials):
//   If Non-Zero(g_UV2Use) UV2 : UV1 (preset useUV2) x g_Tile (preset uvTile) -> Albedo, MaskMap, NormalMap
//   Albedo x Mix(MULTIPLY, Fac) MaskMap.B -> Base Color. The importer sets Fac 1 where it found the mask
//   and 0 where it did not; preset aoScale is that Fac (1 = multiply).
//   MaskMap (sRGB, preset maskSrgb) -> Invert Green -> R Metallic, G Roughness
//   NormalMap (+ DetailNormalMap x g_DetailNormalTile x MaskMap.A when a detail slot is bound) -> Normal
//   Alpha = g_1BitMask ? Binarize(albedo.a) : 1 -> preset opacityMaskClipValue 0.5 / 0
// The graph is shared with MAP_Decal; surface.albedo.a keeps the albedo alpha for it.
SurfaceData Map_PBS00Surface(VS_OUT input, bool isFrontFace)
{
    const float2 baseUV = Node_MaterialUV(input.texcoord, input.texcoord1);
    const float2 uv = baseUV * g_UVTile;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);

    const float4 mask = Node_MaterialMaskOrDefault(uv);
    const float3 separated = Node_InvertGreen(mask.rgb);
    Node_MultiplyCavity(albedo.rgb, separated.b);

    const float3 faceNormal = Node_FrontFaceNormal(input.normal, isFrontFace);
    float3 normal = Node_MaterialNormal(uv, faceNormal, input.tangent);
    normal = Node_AddDetailNormal(normal, baseUV, mask.a, faceNormal, input.tangent);

    SurfaceData surface = Node_DefaultSurface(albedo, normal, input.position.z);
    Node_ApplyMetalRoughness(surface, separated);
    surface.ao = 1.f;                  // B is already multiplied into the albedo
    Node_ApplyMaterialParameters(surface);
    return surface;
}

PS_GBUFFER_OUT PS_PBS00(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    SurfaceData surface = Map_PBS00Surface(input, isFrontFace);
    surface.albedo.a = 1.f;
    return Node_WriteGBuffer(surface);
}

// MAP_Decal - PBS00 materials with g_Decal (road paint, cracks, leaves, stains; plan M4-A):
//   The PBS00 graph, drawn in render group DECAL after every opaque mesh and blended into the G-buffer
//   (BS_GBufferAlphaBlend) by the albedo alpha, so the painted part fades out instead of being cut at 0.5.
//   The preset keeps inverse alpha (black empty DDS texels become transparent) and clip 0.
PS_GBUFFER_OUT PS_Decal(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    return Node_WriteGBufferAlphaBlend(Map_PBS00Surface(input, isFrontFace));
}

// MAP_Plant - Plt00_XXXXX / Plt01_XXXXX (trees and grass cards):
//   Albedo -> Base Color, Albedo.a -> Alpha (HASHED -> preset opacityMaskClipValue 0.5)
//   MaskMap (Non-Color) -> Invert Green -> R Metallic, G Roughness; NormalMap -> Normal. No B multiply.
// MAP_Glass - Gls00_XXXXX (amusement park windows): the .blend builds the same graph (its second normal
//   slot holds the same texture, mixed), and the albedo alpha is opaque, so the preset clips at 0.
// upNormalBlend bends the normal toward world up (0 keeps it). Lit per card, leaf cards came out either
// fully sunlit or ambient black, so neighbouring cards and front/back sheets read as stacked planes.
SurfaceData Map_Plt00Surface(VS_OUT input, bool isFrontFace, float upNormalBlend)
{
    const float2 uv = input.texcoord * g_UVTile;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    const float3 normal = Node_MaterialNormal(uv, Node_FrontFaceNormal(input.normal, isFrontFace), input.tangent);
    SurfaceData surface = Node_DefaultSurface(albedo, lerp(normal, float3(0.f, 1.f, 0.f), upNormalBlend), input.position.z);
    Node_ApplyMetalRoughness(surface, Node_InvertGreen(Node_MaterialMaskOrDefault(uv).rgb));
    Node_ApplyMaterialParameters(surface);
    return surface;
}

PS_GBUFFER_OUT PS_Plant(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    return Node_WriteGBuffer(Map_Plt00Surface(input, isFrontFace, 0.5f));
}

PS_GBUFFER_OUT PS_Glass(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    return Node_WriteGBuffer(Map_Plt00Surface(input, isFrontFace, 0.f));
}

// MAP_PBS10 - PBS10_XXXXX (40 materials, ground and walls with three layers) and PBS11_XXXXX (two layers,
//   the same g_MaterialTile1/2 and g_UV2Use; layer 3 is unbound and skipped):
//   UV1 x g_MaterialTile1 (preset uvTile) -> Albedo1 (g_AlbedoMap), Normal1 (g_NormalMap), MaskMap
//   UV2 x g_MaterialTile2 (preset uvTile2) -> Albedo2/3, Normal2/3
//   Fac1 = height blend(Col.R, Albedo1.a), Fac2 = height blend(Col.G, Albedo2.a)
//   Mix(Mix(Albedo1, Albedo2, Fac1), Albedo3, Fac2) -> Base Color; normals mixed the same way -> Normal Map
//   MaskMap -> Invert Green -> R Metallic, G Roughness; B x Base Color by preset aoScale (as PBS00)
//   A layer whose texture was not extracted (core.dtt) is skipped instead of drawing black.
PS_GBUFFER_OUT PS_PBS10(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    const float2 uv1 = input.texcoord * g_UVTile;
    const float2 uv2 = input.texcoord1 * g_UVTile2;
    const float4 weights = Node_VertexColor(input.color);

    float4 albedo = Node_MaterialBaseColor(uv1);
    float3 normalColor = (g_TextureMask & MATERIAL_TEXTURE_NORMAL) ?
        g_NormalMap.Sample(LinearWrapSampler, uv1).rgb : float3(0.5f, 0.5f, 1.f);
    const float fac1 = Node_HeightBlendFactor(weights.r, albedo.a);
    float layer2Height = 1.f;
    if (Node_IsBound(g_AlbedoMap2))
    {
        const float4 albedo2 = g_AlbedoMap2.Sample(LinearWrapSampler, uv2);
        albedo.rgb = lerp(albedo.rgb, albedo2.rgb, fac1);
        layer2Height = albedo2.a;
        if (Node_IsBound(g_NormalMap2))
            normalColor = lerp(normalColor, g_NormalMap2.Sample(LinearWrapSampler, uv2).rgb, fac1);
    }
    if (Node_IsBound(g_AlbedoMap3))
    {
        const float fac2 = Node_HeightBlendFactor(weights.g, layer2Height);
        albedo.rgb = lerp(albedo.rgb, g_AlbedoMap3.Sample(LinearWrapSampler, uv2).rgb, fac2);
        if (Node_IsBound(g_NormalMap3))
            normalColor = lerp(normalColor, g_NormalMap3.Sample(LinearWrapSampler, uv2).rgb, fac2);
    }
    albedo.a = 1.f;

    const float3 separated = Node_InvertGreen(Node_MaterialMaskOrDefault(uv1).rgb);
    Node_MultiplyCavity(albedo.rgb, separated.b);

    const float3 faceNormal = Node_FrontFaceNormal(input.normal, isFrontFace);
    SurfaceData surface = Node_DefaultSurface(albedo, Node_NormalMap(normalColor, faceNormal, input.tangent), input.position.z);
    Node_ApplyMetalRoughness(surface, separated);
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// MAP_Water - Wtr00_XXXXX / Wtr01_XXXXX (10 materials), opaque as in the .blend:
//   Wtr00: no albedo -> Principled default Base Color 0.8 (preset baseColorTint), Roughness 0.5
//   (preset roughnessOverride), NormalMap1/2/3 mixed 0.5 -> the same single texture -> g_NormalMap.
//   Wtr01: Mix(Albedo2, Albedo1, Albedo1.a) -> Base Color (g_AlbedoMap = Albedo1, g_AlbedoMap2 = Albedo2).
//   The .blend samples water with the default UV (UV1) whatever g_UV2Use says.
// The surface is shared with MAP_WaterBlend (the transparent version, plan M4-B).
// With g_WaterRippleMap bound, two layers of the ripple loop in world XZ (different tile size, drift and frame
// phase, each crossfading between frames) tilt the water normal, so the surface moves without a mesh change.
static const float WATER_RIPPLE_FPS = 15.f;
static const float WATER_RIPPLE_STRENGTH = 0.6f;

// Slope (x, z) of one ripple layer: tile = metres one texture repeat covers, drift in metres per second.
float2 Map_WaterRippleSlope(float2 worldXZ, float tile, float2 drift, float phase, uint frames)
{
    const float frame = (g_Time.x + phase) * WATER_RIPPLE_FPS;
    const float first = floor(frame);
    const float2 uv = (worldXZ + drift * g_Time.x) / tile;
    const float3 a = g_WaterRippleMap.Sample(LinearWrapSampler, float3(uv, fmod(first, frames))).rgb;
    const float3 b = g_WaterRippleMap.Sample(LinearWrapSampler, float3(uv, fmod(first + 1.f, frames))).rgb;
    const float3 normal = lerp(a, b, frame - first) * 2.f - 1.f;
    return normal.xy / max(normal.z, 0.2f);
}

SurfaceData Map_WaterSurface(VS_OUT input, bool isFrontFace)
{
    const float2 uv = input.texcoord * g_UVTile;
    float4 albedo = g_BaseColorTint;
    if (Node_IsBound(g_AlbedoMap))
    {
        const float4 first = g_AlbedoMap.Sample(LinearWrapSampler, uv);
        float3 color = first.rgb;
        if (Node_IsBound(g_AlbedoMap2))
            color = lerp(g_AlbedoMap2.Sample(LinearWrapSampler, uv).rgb, first.rgb, first.a);
        albedo.rgb *= color;
    }
    albedo.a = 1.f;

    float3 normal = Node_MaterialNormal(uv, Node_FrontFaceNormal(input.normal, isFrontFace), input.tangent);
    uint rippleWidth, rippleHeight, rippleFrames;
    g_WaterRippleMap.GetDimensions(rippleWidth, rippleHeight, rippleFrames);
    if (rippleFrames > 0)
    {
        const float2 slope = Map_WaterRippleSlope(input.worldPosition.xz, 3.f, float2(0.12f, 0.05f), 0.f, rippleFrames) +
            Map_WaterRippleSlope(input.worldPosition.xz, 7.f, float2(-0.05f, 0.09f), 0.97f, rippleFrames);
        normal = normalize(normal - WATER_RIPPLE_STRENGTH * float3(slope.x, 0.f, slope.y));
    }
    SurfaceData surface = Node_DefaultSurface(albedo, normal, input.position.z);
    surface.metalness = 0.f;
    Node_ApplyMaterialParameters(surface);
    return surface;
}

PS_GBUFFER_OUT PS_Water(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    return Node_WriteGBuffer(Map_WaterSurface(input, isFrontFace));
}

// How much of the light crossing one metre of water is absorbed (Beer-Lambert): the opacity of the water
// body grows with the water between the surface and the ground below it.
static const float WATER_ABSORPTION = 1.2f;

// MAP_WaterBlend - Wtr00/Wtr01 as transparent water (render group BLEND, plan M4-B):
//   Drawn after the deferred composite, so it lights itself with the same directional light, cascaded shadow
//   and environment reflection as the deferred pass (Node_DirectionalLight, no SSAO).
//   Opacity = absorption over the water thickness (RT_DEPTH behind the pixel minus the surface), at least the
//   Schlick fresnel (F0 0.02) so grazing views show the reflection instead of the bottom.
//   Premultiplied: the lit water body covers alpha of the background and the reflections add on top.
//   The body colour is the preset baseColorTint (NieR g_CamToSurfaceFogColor for Wtr00).
float4 PS_WaterBlend(VS_OUT input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
    const SurfaceData surface = Map_WaterSurface(input, isFrontFace);
    const float2 screenUV = input.clipPosition.xy / input.clipPosition.w * float2(0.5f, -0.5f) + 0.5f;
    const float sceneDepth = g_DepthTexture.SampleLevel(PointClampSampler, screenUV, 0).r;
    const float sceneViewZ = g_ProjMatrix._43 / (sceneDepth - g_ProjMatrix._33);
    const float thickness = max(sceneViewZ - input.clipPosition.w, 0.f);

    const float3 toEye = normalize(g_CameraPosition.xyz - input.worldPosition);
    const float fresnel = 0.02f + 0.98f * pow(1.f - saturate(dot(surface.normal, toEye)), 5.f);
    const float alpha = saturate(max(1.f - exp(-thickness * WATER_ABSORPTION), fresnel));

    float3 shade, specular;
    Node_DirectionalLight(surface.normal, input.worldPosition, surface.albedo.rgb, surface.metalness,
        surface.roughness, 1.f, shade, specular);
    // Same shoulder as the deferred composite, so a sun brighter than 1 burns the water like the ground around it.
    return float4(Node_ToneMap(surface.albedo.rgb * shade * alpha + specular), alpha);
}

// MAP_Emissive - CNS00_XXXXX lamps, light strips and neon (render group BLEND):
//   Unlit: g_AlbedoMap x g_AlbedoColor (preset emissiveColor) x g_Intensity (preset emissiveIntensity). NieR
//   g_bAlbedoOverWrite materials have no albedo bound and glow in the flat colour.
//   Drawn after the deferred composite, so a night level's dim light does not darken it; it stays out of the
//   G-buffer and the shadow pass. The shoulder runs per channel like the lit scene's, so a bright orange lamp or
//   window (light_tex_aaax_ (1, 0.31, 0) x 8) saturates to the yellow-white core the original shows.
float4 PS_Emissive(VS_OUT input) : SV_TARGET0
{
    float3 color = g_EmissiveColor.rgb * g_EmissiveIntensity;
    if (Node_IsBound(g_AlbedoMap))
        color *= g_AlbedoMap.Sample(LinearWrapSampler, Node_MaterialUV(input.texcoord, input.texcoord1) * g_UVTile).rgb;
    return float4(Node_ToneMap(color), 1.f);
}

// ShadowCompatible: cascaded shadow map depth (Renderer shadow pass). Same UV as the colour passes; the
// material's clip value decides the holes (Plant/decals 0.5, PBS10 0 because its alpha is height).
void PS_ShadowDepth(VS_OUT input)
{
    Node_ShadowDepthClip(Node_MaterialUV(input.texcoord, input.texcoord1) * g_UVTile);
}

technique11 DefaultTechnique
{
    // A tile model without a preset binds only its converted albedo/mask (Model::BindAndBeginMaterial
    // needs Default_Pass then); it draws as PBS00 with the default MaterialBuffer.
    pass Default_Pass
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_PBS00();
    }
    pass MAP_PBS00
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_PBS00();
    }
    pass MAP_Plant
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Plant();
    }
    pass MAP_PBS10
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_PBS10();
    }
    pass MAP_Water
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Water();
    }
    // Render group BLEND: forward, after the deferred composite; depth test without write, premultiplied alpha.
    pass MAP_WaterBlend
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_PremultipliedAlpha, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_WaterBlend();
    }
    // Render group BLEND: forward and opaque over the lit screen; writes depth like the surface it replaces.
    pass MAP_Emissive
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Emissive();
    }
    // Render group DECAL: depth test without write, G-buffer alpha blend.
    pass MAP_Decal
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_GBufferAlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Decal();
    }
    pass ShadowCompatible
    {
        SetRasterizerState(RS_ShadowDepth);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_ShadowDepth();
    }
    pass MAP_Glass
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Glass();
    }
}
