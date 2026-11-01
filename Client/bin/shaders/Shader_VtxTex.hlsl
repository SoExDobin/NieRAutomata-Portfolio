#include "EngineShaderDefine.hlsli"

float4x4 g_WorldMatrix;
float4x4 g_ViewMatrix;
float4x4 g_ProjMatrix;
texture2D g_Texture;
float4 g_RGBA = float4(1.f, 1.f, 1.f, 1.f);
float4 g_LoadingLogoUVRect = float4(0.f, 0.f, 1.f, 1.f);
float g_LoadingEffectTime = 0.f;
float g_HpRatio = 1.f;
float2 g_TitleIntro = float2(0.f, 0.f); // elapsed seconds, fading strength

struct VS_IN { float3 position : POSITION; float2 texcoord : TEXCOORD0; };
struct VS_OUT { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = mul(float4(input.position, 1.f), mul(mul(g_WorldMatrix, g_ViewMatrix), g_ProjMatrix));
    output.texcoord = input.texcoord;
    return output;
}

float4 PS_Default(VS_OUT input) : SV_TARGET0
{
    return g_Texture.Sample(LinearClampSampler, input.texcoord) * g_RGBA;
}

float4 PS_HpBar(VS_OUT input) : SV_TARGET0
{
    if (input.texcoord.x > saturate(g_HpRatio)) discard;
    return g_Texture.Sample(LinearClampSampler, input.texcoord) * g_RGBA;
}

float4 PS_LoadingLogo(VS_OUT input) : SV_TARGET0
{
    float2 uv = lerp(g_LoadingLogoUVRect.xy, g_LoadingLogoUVRect.zw, input.texcoord);
    float phase = fmod(g_LoadingEffectTime, 1.35f);
    float band = floor(input.texcoord.y * 32.f);
    float burst = step(0.88f, phase) * (1.f - step(0.98f, phase));
    float tear = sin(band * 7.1f + floor(g_LoadingEffectTime * 48.f));
    uv.x += burst * step(0.55f, tear) * tear * 0.006f;
    uv = clamp(uv, g_LoadingLogoUVRect.xy, g_LoadingLogoUVRect.zw);
    float2 shift = float2(0.00045f + burst * 0.0012f, 0.f);
    float4 centre = g_Texture.Sample(LinearClampSampler, uv);
    float4 red = g_Texture.Sample(LinearClampSampler, clamp(uv - shift, g_LoadingLogoUVRect.xy, g_LoadingLogoUVRect.zw));
    float4 cyan = g_Texture.Sample(LinearClampSampler, clamp(uv + shift, g_LoadingLogoUVRect.xy, g_LoadingLogoUVRect.zw));
    float alpha = max(centre.a, max(red.a, cyan.a));
    float3 rgb = float3(red.r * red.a, cyan.g * cyan.a, cyan.b * cyan.a) / max(alpha, 0.0001f);
    return float4(rgb, alpha) * g_RGBA;
}

float TitleGlitchHash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 PS_TitleIntro(VS_OUT input) : SV_TARGET0
{
    const float strength = g_TitleIntro.y;
    if (strength <= 0.f)
        return PS_Default(input);

    const float tick = floor(g_TitleIntro.x * 24.f);
    const float band = floor(input.texcoord.y * 144.f);
    const float tear = step(1.f - 0.5f * strength, TitleGlitchHash(float2(band, tick)));
    float2 uv = input.texcoord;
    uv.x += (TitleGlitchHash(float2(tick, band)) * 2.f - 1.f) * 0.03f * strength * tear;
    const float2 split = float2(0.006f * strength * (0.5f + TitleGlitchHash(float2(tick, -1.f))), 0.f);
    const float4 centre = g_Texture.Sample(LinearClampSampler, uv);
    const float4 red = g_Texture.Sample(LinearClampSampler, uv + split);
    const float4 blue = g_Texture.Sample(LinearClampSampler, uv - split);
    return float4(red.r, centre.g, blue.b, centre.a) * g_RGBA;
}

technique11 DefaultTechnique
{
    pass Default
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Default();
    }
    pass HpBar
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_HpBar();
    }
    pass LoadingLogo
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_LoadingLogo();
    }
    pass TitleIntro
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_TitleIntro();
    }
}
