#include "EngineShaderDefine.hlsli"
#include "EngineShaderVertex.hlsli"

// The in-game weapon halo is core.eff/093 (092 is the half-size one). Halo_core093.model holds its ring emitters
// in effect units, one mesh each: 0 circuit, 1 lettering (both on the flat torus ef0008), 2 streak (the thin
// cylinder band ef0041). Record 0, a faint camera-facing glow, is drawn from a quad. Record 1 (007/7 on a sphere)
// is left out: it reads as a bubble around the hilt, which the in-game captures do not show.
// Colours and strengths are the .est MOVE rgb and alpha, added on top of the scene.
Texture2D g_HaloLetteringMap;   // 041/1
Texture2D g_HaloCircuitMap;     // 041/0
Texture2D g_HaloStreakMap;      // 306/31
Texture2D g_HaloGlowMap;        // 007/2
float g_HaloSpin;               // this layer's turn about the ring axis (Z), radians
float g_BladeStart;
float g_BladeEnd;
float g_TransitionWidth;
float g_TransitionIntensity;

static const float GlowSize = 0.61f;
// The rings are a few pixels wide on screen; a lower mip keeps the glyph strokes apart.
static const float RingMipBias = -0.5f;
// Raised over the .est strengths on request (2026-10-05) for a stronger glow.
static const float HaloEmissive = 1.8f;

typedef VTXMESH VS_IN;
struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float s, c;
    sincos(g_HaloSpin, s, c);
    float3 position = float3(input.position.x * c - input.position.y * s,
                             input.position.x * s + input.position.y * c, input.position.z);
    output.position = Node_ObjectToClip(position);
    output.uv = input.texcoord;
    return output;
}

// A unit quad (xy in -0.5..0.5) turned to the camera at the halo centre, keeping the halo's world scale.
VS_OUT VS_Glow(VS_IN input)
{
    VS_OUT output;
    float scale = length(g_WorldMatrix[0].xyz) * GlowSize;
    float3 position = g_WorldMatrix[3].xyz +
        (g_ViewInverseMatrix[0].xyz * input.position.x + g_ViewInverseMatrix[1].xyz * input.position.y) * scale;
    output.position = mul(mul(float4(position, 1.f), g_ViewMatrix), g_ProjMatrix);
    output.uv = input.texcoord;
    return output;
}

// A camera-facing ribbon follows the actual bone0 blade axis of wp0070 or wp0220.
VS_OUT VS_TransitionBlade(VS_IN input)
{
    VS_OUT output;
    float3 axis = normalize(g_WorldMatrix[2].xyz);
    float3 across = cross(axis, g_ViewInverseMatrix[2].xyz);
    if (dot(across, across) < 0.01f) across = g_ViewInverseMatrix[0].xyz;
    across = normalize(across);
    float z = lerp(g_BladeStart, g_BladeEnd, input.texcoord.y);
    float3 position = g_WorldMatrix[3].xyz + axis * z + across * input.position.x * g_TransitionWidth;
    output.position = mul(mul(float4(position, 1.f), g_ViewMatrix), g_ProjMatrix);
    output.uv = input.texcoord;
    return output;
}

// The glyph images are alpha masks (041/0 repeats its alpha in RGB); multiplying both would square thin strokes away.
float4 PS_Circuit(VS_OUT input) : SV_TARGET0
{
    float mask = g_HaloCircuitMap.SampleBias(LinearWrapSampler, input.uv, RingMipBias).a;
    return float4(float3(1.3f, 1.1f, 0.9f) * 3.8f * HaloEmissive * mask, 0.f);
}

// The record's alpha is 0.3; raised to 1 so the lettering reads at the halo's on-screen size.
float4 PS_Lettering(VS_OUT input) : SV_TARGET0
{
    float mask = g_HaloLetteringMap.SampleBias(LinearWrapSampler, input.uv, RingMipBias).a;
    return float4(float3(1.3f, 1.1f, 0.9f) * HaloEmissive * mask, 0.f);
}

float4 PS_Streak(VS_OUT input) : SV_TARGET0
{
    float4 texel = g_HaloStreakMap.Sample(LinearWrapSampler, input.uv);
    return float4(texel.rgb * texel.a * float3(3.3f, 2.8f, 2.3f) * 0.3f * HaloEmissive, 0.f);
}

float4 PS_Glow(VS_OUT input) : SV_TARGET0
{
    float4 texel = g_HaloGlowMap.Sample(LinearClampSampler, input.uv);
    return float4(texel.rgb * texel.a * float3(1.6f, 1.4f, 0.9f) * 0.07f * HaloEmissive, 0.f);
}

float4 PS_TransitionBlade(VS_OUT input) : SV_TARGET0
{
    float centre = saturate(1.f - abs(input.uv.x * 2.f - 1.f));
    float taper = saturate(min(input.uv.y, 1.f - input.uv.y) * 8.f);
    float glow = pow(centre, 2.f) * taper * g_TransitionIntensity;
    return float4(float3(1.0f, 0.88f, 0.63f) * glow * 5.f, 0.f);
}

float4 PS_TransitionGlint(VS_OUT input) : SV_TARGET0
{
    float4 texel = g_HaloGlowMap.Sample(LinearClampSampler, input.uv);
    return float4(float3(1.f, 0.9f, 0.7f) * texel.a * g_TransitionIntensity * 7.f, 0.f);
}

// Additive light, depth-tested without depth writes, so one Halo cannot cut another's light. The rings keep
// back-face culling so the flattened torus shows one face at a time.
#define HALO_PASS(name, rs, vs, ps) \
    pass name \
    { \
        SetRasterizerState(rs); \
        SetDepthStencilState(DSS_TestNoWrite, 0); \
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff); \
        VertexShader = compile vs_5_0 vs(); \
        PixelShader = compile ps_5_0 ps(); \
    }

technique11 DefaultTechnique
{
    HALO_PASS(Circuit, RS_Default, VS_Main, PS_Circuit)
    HALO_PASS(Lettering, RS_Default, VS_Main, PS_Lettering)
    HALO_PASS(Streak, RS_Default, VS_Main, PS_Streak)
    HALO_PASS(Glow, RS_CullNone, VS_Glow, PS_Glow)
    HALO_PASS(TransitionBlade, RS_CullNone, VS_TransitionBlade, PS_TransitionBlade)
    HALO_PASS(TransitionGlint, RS_CullNone, VS_Glow, PS_TransitionGlint)
}
