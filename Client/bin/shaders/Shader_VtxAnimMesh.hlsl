#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"
#include "EngineShaderMaterialTexture.hlsli"
#include "EngineShaderVertex.hlsli"

// Player texture slot beyond the engine's (pl0000.json "textures" keys; g_DetailNormalMap is an engine slot).
//   W_PBS  : g_DetailNormalMap = 1DC3217F
//   W_Skin : g_DetailNormalMap = 6E40EC01, g_CurvatureMap = 0906BE84
Texture2D g_CurvatureMap;
float4 g_GhostColor = float4(1.f, 1.f, 1.f, 1.f);
float g_GhostAlpha = 1.f;

// Every material value comes from the material settings "parameters" through MaterialBuffer:
// g_BaseColorTint, g_EmissiveColor/Intensity, g_OpacityMaskClipValue, g_NormalStrength,
// g_Metallic/g_Roughness overrides (g_MaterialFlags), metalness/roughness/AO scales,
// g_DetailNormalTile, g_DetailNormalWeight, g_Intensity. The passes hold no material constants.

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
};

VS_OUT VS_Main(VTXANIMMESH input)
{
    VS_OUT output;

    const matrix skin = Node_SkinMatrix(input);
    output.position = Node_ObjectToClip(mul(float4(input.position, 1.f), skin));
    output.normal = Node_ObjectToWorldDirection(mul(float4(input.normal, 0.f), skin).xyz);
    output.tangent = Node_ObjectToWorldDirection(mul(float4(input.tangent, 0.f), skin).xyz);
    output.texcoord = input.texcoord;

    return output;
}

// Default_Pass / AlphaBlend / ShadowCompatible: models without settings, and the generic mask graph.
PS_GBUFFER_OUT PS_Main(VS_OUT input)
{
    float4 albedo = Node_MaterialBaseColor(input.texcoord);
    Node_OpacityClip(albedo.a);

    SurfaceData surface = Node_DefaultSurface(albedo, input.normal, input.position.z);
    Node_ApplyNierMask(surface, Node_MaterialMask(input.texcoord).rgb);
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// PL_PBS - W_PBS (PBS00_XXXXX)
//   Albedo x MaskMap.B -> Base Color, MaskMap -> Invert Green -> R Metallic, G Roughness
//   NormalMap + DetailNormalMap(x g_DetailNormalTile) x MaskMap.A x g_DetailNormalWeight -> Normal
//   The .blend reads this mask as sRGB, so the preset sets maskSrgb (Node_MaterialMask decodes RGB).
//   Weapons use the same PBS00 graph in Shader_VtxAnimMesh_Weapon.hlsl (pass WP_PBS).
PS_GBUFFER_OUT PS_PBS(VS_OUT input)
{
    const float2 uv = input.texcoord;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    const float4 mask = Node_MaterialMaskOrDefault(uv);
    const float3 separated = Node_InvertGreen(mask.rgb);
    Node_MultiplyCavity(albedo.rgb, separated.b);

    float3 normal = Node_MaterialNormal(uv, input.normal, input.tangent);
    normal = Node_AddDetailNormal(normal, uv, mask.a, input.normal, input.tangent);

    SurfaceData surface = Node_DefaultSurface(albedo, normal, input.position.z);
    Node_ApplyMetalRoughness(surface, separated);
    surface.ao = 1.f;                  // B is already multiplied into the albedo
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// PL_Cloth - W_Cloth, W_Cloth_satin, W_Cloth_tights (CLT00_XXXXX)
//   Albedo -> Base Color, MaskMap -> Invert Green -> R Metallic, G Roughness, NormalMap -> Normal
//   As in pl0000.blend: mask R drives Metallic (no override), two-sided (backface culling off).
PS_GBUFFER_OUT PS_Cloth(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    const float2 uv = input.texcoord;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    // Two-sided (the skirt and sleeves are open meshes); inner faces use the flipped normal.
    const float3 faceNormal = Node_FrontFaceNormal(input.normal, isFrontFace);
    SurfaceData surface = Node_DefaultSurface(albedo, Node_MaterialNormal(uv, faceNormal, input.tangent), input.position.z);
    if (g_TextureMask & MATERIAL_TEXTURE_ORM)
        Node_ApplyMetalRoughness(surface, Node_InvertGreen(Node_MaterialMask(uv).rgb));
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// PL_Skin - W_Skin (SKN00_XXXXX)
//   Albedo -> Base Color, MaskMap -> Invert Green -> R Metallic, G Roughness
//   Mix(DetailNormal x g_DetailNormalTile, NormalMap, g_DetailNormalWeight) -> Normal Map -> Normal
//   CurvatureMap R x G -> Subsurface weight, stored in RT_MATERIAL.a (lighting does not read it yet)
// PL_A2_Skin - A2 skins (PL_0100_Skin1, PL_0000_Skin in pl0100): the same graph; the A2 preset sets the
//   source g_DetailNormalTile (50 / 20), which the 2B preset leaves at 1.
PS_GBUFFER_OUT PS_Skin(VS_OUT input)
{
    const float2 uv = input.texcoord;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    float3 normal = normalize(input.normal);
    if (g_TextureMask & MATERIAL_TEXTURE_NORMAL)
    {
        float3 color = g_NormalMap.Sample(LinearWrapSampler, uv).rgb;
        if (Node_IsBound(g_DetailNormalMap))
            color = lerp(g_DetailNormalMap.Sample(LinearWrapSampler, uv * g_DetailNormalTile).rgb, color, g_DetailNormalWeight);
        normal = Node_NormalMap(color, input.normal, input.tangent);
    }

    SurfaceData surface = Node_DefaultSurface(albedo, normal, input.position.z);
    if (g_TextureMask & MATERIAL_TEXTURE_ORM)
        Node_ApplyMetalRoughness(surface, Node_InvertGreen(Node_MaterialMask(uv).rgb));
    Node_ApplyMaterialParameters(surface);

    float subsurface = 0.f;
    if (Node_IsBound(g_CurvatureMap))
    {
        const float3 curvature = g_CurvatureMap.Sample(LinearWrapSampler, uv).rgb;
        subsurface = curvature.r * curvature.g;
    }

    PS_GBUFFER_OUT output = Node_WriteGBuffer(surface);
    output.material.a = subsurface;
    return output;
}

// PL_Hair - W_HairEyelash, W_Hair00, W_Hair02 (Hair01_XXXXX)
//   Albedo -> Base Color, Albedo.A -> alpha test (preset clip 0.5 = g_1BitMask)
//   MaskMap Color -> Metallic, Roughness 0.5 (Principled default), as in pl0000.blend (no overrides).
//   Two-sided: the normal faces the camera (Node_TwoSidedNormal), not SV_IsFrontFace.
PS_GBUFFER_OUT PS_Hair(VS_OUT input)
{
    const float2 uv = input.texcoord;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    const float3 faceNormal = Node_TwoSidedNormal(input.normal);
    SurfaceData surface = Node_DefaultSurface(albedo, Node_MaterialNormal(uv, faceNormal, input.tangent), input.position.z);
    if (g_TextureMask & MATERIAL_TEXTURE_ORM)
        surface.metalness = Node_MaterialMask(uv).r;
    surface.roughness = 0.5f;
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// PL_Eye - PL_0000_Eye (Eye00_XXXXX)
//   Albedo -> Base Color, MaskMap -> Invert Green -> R Metallic, G Roughness, NormalMap -> Normal
//   pl0000.blend uses the mask values directly (no overrides).
// PL_PBS20 - A2_Hotshader_shl000 (PBS20_XXXXX, A2 body): pl0100.blend builds the same graph (mask
//   Non-Color, no B multiply, no detail normal), so it shares PS_Eye. g_SpreadPatternMap is not in the .blend.
PS_GBUFFER_OUT PS_Eye(VS_OUT input)
{
    const float2 uv = input.texcoord;
    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    SurfaceData surface = Node_DefaultSurface(albedo, Node_MaterialNormal(uv, input.normal, input.tangent), input.position.z);
    if (g_TextureMask & MATERIAL_TEXTURE_ORM)
        Node_ApplyMetalRoughness(surface, Node_InvertGreen(Node_MaterialMask(uv).rgb));
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// PL_Multiply - eyeshadow1 (CNS00_XXXXX, Technique "Multiplication")
//   Albedo x tint multiplies the skin albedo already in RT_DIFFUSE (BS_GBufferMultiply),
//   scaled by g_Intensity. Targets 1-3 are write-masked, so the skin normal/material/depth stay.
PS_GBUFFER_OUT PS_Multiply(VS_OUT input)
{
    const float4 albedo = g_AlbedoMap.Sample(LinearWrapSampler, input.texcoord) * g_BaseColorTint;
    return Node_WriteGBufferMultiply(albedo.rgb, albedo.a * g_Intensity);
}

// ShadowCompatible: cascaded shadow map depth (Renderer shadow pass), material alpha clip only.
void PS_ShadowDepth(VS_OUT input)
{
    Node_ShadowDepthClip(input.texcoord);
}

float4 PS_Ghost(VS_OUT input) : SV_TARGET0
{
    const float4 albedo = Node_MaterialBaseColor(input.texcoord);
    Node_OpacityClip(albedo.a);
    const float alpha = saturate(albedo.a * g_GhostColor.a * g_GhostAlpha);
    clip(alpha - 0.01f);
    const float detail = dot(albedo.rgb, float3(0.299f, 0.587f, 0.114f));
    const float3 color = g_GhostColor.rgb * lerp(0.85f, 1.f, saturate(detail));
    return float4(color, alpha);
}

// Shatter: a dying machine (Em0010Body::Begin_Shatter) breaks into its parts. The pose is frozen and every
// vertex follows the part of its dominant bone: the part's posed centre (bind-space g_PartCenters through that
// bone's skinning matrix) flies out from g_ShatterCenter (world), tumbles, falls, scrapes to a stop on the ground the
// body stood on, and from ShatterDissolveStart is eaten away by the Burn noise (each part a little later than the last,
// a hot band along the line) until g_ShatterDuration. PS_Shatter (after the Burn section, whose noise it uses) is PS_Main
// plus the dissolve, so the parts keep their material.
float g_ShatterTime;
float g_ShatterDuration;        // s; every user binds its own (the effect is shared): small machines 1.5, Em3000 longer
float3 g_ShatterCenter;
uint g_ShatterSeed;             // per mesh, so equal palette slots of different meshes fly differently
float4 g_PartCenters[256];      // Model::Get_MeshPartCenters, palette order

static const float ShatterDissolveStart = 0.6f;   // share of g_ShatterDuration
static const float ShatterEdgeWidth = 0.05f;    // noise above the line that glows hot
static const float3 ShatterEdgeColor = float3(1.f, 0.6f, 0.25f);
static const float ShatterGroundOffset = 1.f;  // g_ShatterCenter is this far above the feet
static const float ShatterFriction = 6.f;      // m/s^2 slowing a landed part's slide

float Shatter_Hash(uint n)
{
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / 2147483647.f;
}

// Rotation of v about a unit axis (Rodrigues).
float3 Shatter_Rotate(float3 v, float3 axis, float angle)
{
    float s, c;
    sincos(angle, s, c);
    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.f - c);
}

struct VS_SHATTER_OUT
{
    float3 position : POSITION;     // world, frozen pose
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    nointerpolation uint part : TEXCOORD1;
};

struct GS_SHATTER_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    float dissolve : TEXCOORD1;     // 0 whole .. 1 gone
};

VS_SHATTER_OUT VS_Shatter(VTXANIMMESH input)
{
    VS_SHATTER_OUT output;

    float4 weights = input.blendWeight;
    weights.w = 1.f - (weights.x + weights.y + weights.z);
    uint part = input.blendIndex.x;
    float heaviest = weights.x;
    if (weights.y > heaviest) { heaviest = weights.y; part = input.blendIndex.y; }
    if (weights.z > heaviest) { heaviest = weights.z; part = input.blendIndex.z; }
    if (weights.w > heaviest) { heaviest = weights.w; part = input.blendIndex.w; }

    const matrix skin = Node_SkinMatrix(input);
    output.position = mul(mul(float4(input.position, 1.f), skin), g_WorldMatrix).xyz;
    output.normal = Node_ObjectToWorldDirection(mul(float4(input.normal, 0.f), skin).xyz);
    output.tangent = Node_ObjectToWorldDirection(mul(float4(input.tangent, 0.f), skin).xyz);
    output.texcoord = input.texcoord;
    output.part = part;
    return output;
}

// A triangle flies whole with one part: on a smoothly skinned joint its corners lean on different bones, and moving
// each corner with its own part stretched the triangle between two flying pieces (em0000, em3004). The part two corners
// share wins; three different parts take the first corner's.
[maxvertexcount(3)]
void GS_Shatter(triangle VS_SHATTER_OUT input[3], inout TriangleStream<GS_SHATTER_OUT> stream)
{
    const uint part = (input[1].part == input[2].part) ? input[1].part : input[0].part;
    const float3 pivot = mul(mul(float4(g_PartCenters[part].xyz, 1.f), g_BoneMatrices[part]), g_WorldMatrix).xyz;

    const uint seed = (g_ShatterSeed * 512u + part) * 3u;
    const float3 random = float3(Shatter_Hash(seed), Shatter_Hash(seed + 1u), Shatter_Hash(seed + 2u));

    const float t = g_ShatterTime;
    float3 outward = pivot - g_ShatterCenter;
    outward.y = 0.f;
    outward = dot(outward, outward) > 1e-6f ? normalize(outward) : float3(1.f, 0.f, 0.f);
    const float3 velocity = normalize(outward + (random - 0.5f) * 0.9f + float3(0.f, 1.1f, 0.f)) * lerp(3.f, 7.f, random.x);
    // The parts land where the body stood instead of falling through the floor: each flies until its centre comes down
    // to the ground, then stops tumbling and scrapes along it, slowed by ShatterFriction to a stop.
    const float ground = g_ShatterCenter.y - ShatterGroundOffset + 0.05f;
    const float drop = max(pivot.y - ground, 0.f);
    const float landing = (velocity.y + sqrt(velocity.y * velocity.y + 19.6f * drop)) / 9.8f;
    const float flight = min(t, landing);
    float3 offset = velocity * flight + float3(0.f, -4.9f, 0.f) * flight * flight;
    const float glide = length(velocity.xz);
    const float slide = min(max(t - landing, 0.f), glide / ShatterFriction);
    if (glide > 1e-4f)
        offset.xz += velocity.xz / glide * (glide * slide - 0.5f * ShatterFriction * slide * slide);
    offset.y = max(offset.y, ground - pivot.y);

    const float3 axis = normalize(random * 2.f - 1.f + 0.001f);
    const float angle = lerp(4.f, 12.f, random.y) * flight;
    // Each part starts dissolving up to 0.15 of the lifetime after the first.
    const float dissolveFrom = ShatterDissolveStart + random.z * 0.15f;
    const float dissolve = saturate((t / g_ShatterDuration - dissolveFrom) / (1.f - dissolveFrom - 0.02f));

    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);
    [unroll]
    for (uint i = 0; i < 3; ++i)
    {
        GS_SHATTER_OUT output;
        const float3 world = pivot + offset + Shatter_Rotate(input[i].position - pivot, axis, angle);
        output.position = mul(float4(world, 1.f), viewProj);
        output.normal = normalize(Shatter_Rotate(input[i].normal, axis, angle));
        output.tangent = normalize(Shatter_Rotate(input[i].tangent, axis, angle));
        output.texcoord = input[i].texcoord;
        output.dissolve = dissolve;
        stream.Append(output);
    }
}

// Crack: a dying machine (Em0010Body::Begin_Crack) glows with orange fissures that spread over its body
// before it bursts. Drawn a second time additively in the late forward pass (WORLDUI) with VS_Main, so it
// matches the lit body's depth. The fissures are Voronoi cell borders in texture space; a cell lights up once
// g_CrackProgress passes its random threshold (at most CrackCoverage of them), and UV seams break the lines
// into short dashes.
float g_CrackProgress;
float g_CrackTime;

static const float CrackCellsPerUV = 10.f;
static const float CrackWidth = 0.03f;
static const float CrackCoverage = 0.35f;   // at most this share of the cells ever lights up
static const float3 CrackCoreColor = float3(2.4f, 1.9f, 1.f);
static const float3 CrackGlowColor = float3(1.4f, 0.5f, 0.12f);

float2 Crack_Hash2(float2 p)
{
    p = float2(dot(p, float2(127.1f, 311.7f)), dot(p, float2(269.5f, 183.3f)));
    return frac(sin(p) * 43758.5453f);
}

// Distance to the nearest Voronoi cell border, and the id of the cell the point lies in.
float Crack_EdgeDistance(float2 p, out float2 cellId)
{
    const float2 cell = floor(p);
    const float2 f = frac(p);

    float2 nearest = 0.f;
    float best = 8.f;
    cellId = cell;
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        const float2 g = float2(x, y);
        const float2 r = g + Crack_Hash2(cell + g) - f;
        const float d = dot(r, r);
        if (d < best)
        {
            best = d;
            nearest = r;
            cellId = cell + g;
        }
    }

    best = 8.f;
    [unroll] for (int j = -2; j <= 2; ++j)
    [unroll] for (int i = -2; i <= 2; ++i)
    {
        const float2 g = float2(i, j);
        const float2 r = g + Crack_Hash2(cell + g) - f;
        const float2 between = r - nearest;
        if (dot(between, between) > 1e-5f)
            best = min(best, dot(0.5f * (nearest + r), normalize(between)));
    }
    return best;
}

float4 PS_Crack(VS_OUT input) : SV_TARGET0
{
    float2 cellId;
    const float edge = Crack_EdgeDistance(input.texcoord * CrackCellsPerUV, cellId);
    const float2 random = Crack_Hash2(cellId + 17.f);

    const float reveal = saturate((g_CrackProgress * CrackCoverage - random.x) * 12.f);
    const float core = 1.f - smoothstep(0.f, CrackWidth, edge);
    const float glow = 1.f - smoothstep(0.f, CrackWidth * 4.f, edge);
    const float flicker = 0.75f + 0.25f * sin(g_CrackTime * 37.f + random.y * 6.2832f);

    const float3 color = (CrackCoreColor * core + CrackGlowColor * glow * 0.3f) * reveal * flicker;
    clip(dot(color, 1.f) - 0.01f);
    return float4(color, 0.f);
}

// Burn / BurnEdge: cloth burning away (Em3000Parts::Begin_Burn, em3000's head cloth and skirt at the phase 1 stagger).
// Noise in texture space rises past g_BurnProgress (0 whole .. 1 gone): Burn draws the PBS surface where the noise is
// still above it, charred black just above the line; BurnEdge adds the glowing band along the line into RT_EMISSIVE
// (RENDERGROUP::EMISSIVE, as EM_Eye), so it blooms. Both are two-sided like the cloth.
float g_BurnProgress;

static const float BurnNoisePerUV = 9.f;
static const float BurnCharWidth = 0.08f;      // noise above the line that is charred
static const float BurnEdgeWidth = 0.035f;     // noise above the line that glows
static const float3 BurnEdgeColor = float3(3.2f, 1.1f, 0.25f);

float Burn_Noise(float2 uv)
{
    // Two octaves of value noise; Crack_Hash2 gives each lattice corner its value.
    float total = 0.f;
    float amplitude = 0.65f;
    float2 p = uv * BurnNoisePerUV;
    [unroll] for (int octave = 0; octave < 2; ++octave)
    {
        const float2 cell = floor(p);
        const float2 f = frac(p);
        const float2 s = f * f * (3.f - 2.f * f);
        const float a = Crack_Hash2(cell).x;
        const float b = Crack_Hash2(cell + float2(1.f, 0.f)).x;
        const float c = Crack_Hash2(cell + float2(0.f, 1.f)).x;
        const float d = Crack_Hash2(cell + float2(1.f, 1.f)).x;
        total += amplitude * lerp(lerp(a, b, s.x), lerp(c, d, s.x), s.y);
        amplitude *= 0.54f;
        p *= 2.7f;
    }
    // Spread to about 0..1 so progress 1 leaves nothing.
    return saturate(total);
}

// How far the noise is above the burn line: < 0 is burnt away.
float Burn_Remain(float2 uv)
{
    return Burn_Noise(uv) - lerp(-BurnCharWidth, 1.f + BurnEdgeWidth, g_BurnProgress);
}

PS_GBUFFER_OUT PS_Burn(VS_OUT input, bool isFrontFace : SV_IsFrontFace)
{
    const float2 uv = input.texcoord;
    const float remain = Burn_Remain(uv);
    clip(remain);

    float4 albedo = Node_MaterialBaseColor(uv);
    Node_OpacityClip(albedo.a);
    albedo.a = 1.f;

    const float4 mask = Node_MaterialMaskOrDefault(uv);
    const float3 separated = Node_InvertGreen(mask.rgb);
    Node_MultiplyCavity(albedo.rgb, separated.b);
    albedo.rgb *= smoothstep(0.f, BurnCharWidth, remain);

    const float3 faceNormal = Node_FrontFaceNormal(input.normal, isFrontFace);
    float3 normal = Node_MaterialNormal(uv, faceNormal, input.tangent);
    normal = Node_AddDetailNormal(normal, uv, mask.a, faceNormal, input.tangent);

    SurfaceData surface = Node_DefaultSurface(albedo, normal, input.position.z);
    Node_ApplyMetalRoughness(surface, separated);
    surface.ao = 1.f;
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

float4 PS_BurnEdge(VS_OUT input) : SV_TARGET0
{
    Node_OpacityClip(Node_MaterialBaseColor(input.texcoord).a);     // no edge over the cloth's cut-out holes
    const float remain = Burn_Remain(input.texcoord);
    clip(remain);
    const float glow = 1.f - smoothstep(0.f, BurnEdgeWidth, remain);
    clip(glow - 0.01f);
    return float4(BurnEdgeColor * glow, 1.f);
}

PS_GBUFFER_OUT PS_Shatter(GS_SHATTER_OUT input)
{
    // The Burn noise eats the part away; the band about to go glows hot.
    const float remain = Burn_Noise(input.texcoord) - lerp(-ShatterEdgeWidth, 1.f + ShatterEdgeWidth, input.dissolve);
    clip(remain);

    float4 albedo = Node_MaterialBaseColor(input.texcoord);
    Node_OpacityClip(albedo.a);
    if (input.dissolve > 0.f && remain < ShatterEdgeWidth)
        albedo.rgb = ShatterEdgeColor;

    SurfaceData surface = Node_DefaultSurface(albedo, input.normal, input.position.z);
    Node_ApplyNierMask(surface, Node_MaterialMask(input.texcoord).rgb);
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// Pass 0-2 keep their original order: Phase5GateVerifier checks Default_Pass, AlphaBlend and
// ShadowCompatible by index. New passes go after them and are looked up by name.
// EM_Eye: a machine's eyes (EmTelegraph::Render_Eyes) drawn again into RT_EMISSIVE (RENDERGROUP::EMISSIVE) with
// VS_Main, so the glow is the skinned eye itself and moves with it. The material's g_EmissiveMap marks the eyes
// (authored per model); a mesh without one draws nothing. g_EyeColor is HDR: the grade adds it and blooms it.
float4 g_EyeColor;

float4 PS_EmEye(VS_OUT input) : SV_TARGET0
{
    if (!(g_TextureMask & MATERIAL_TEXTURE_EMISSIVE))
        discard;
    const float mask = g_EmissiveMap.Sample(LinearClampSampler, input.texcoord).r;
    clip(mask - 0.004f);
    return float4(g_EyeColor.rgb * mask, 1.f);
}

technique11 DefaultTechnique
{
    pass Default_Pass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Main();
    }
    pass AlphaBlend
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Main();
    }
    pass ShadowCompatible
    {
        SetRasterizerState(RS_ShadowDepth);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_ShadowDepth();
    }
    pass PL_PBS
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_PBS();
    }
    pass PL_Cloth
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Cloth();
    }
    pass PL_Skin
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Skin();
    }
    pass PL_Hair
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Hair();
    }
    pass PL_Eye
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Eye();
    }
    pass PL_Multiply
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_GBufferMultiply, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Multiply();
    }
    pass PL_PBS20
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Eye();
    }
    pass PL_A2_Skin
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Skin();
    }
    pass PL_Ghost
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Ghost();
    }
    pass Shatter
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Shatter();
        GeometryShader = compile gs_5_0 GS_Shatter();
        PixelShader = compile ps_5_0 PS_Shatter();
    }
    pass Crack
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Crack();
    }
    pass EM_Eye
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_EmEye();
    }
    pass Burn
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Burn();
    }
    pass BurnEdge
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_BurnEdge();
    }
}
