#include "EngineShaderDefine.hlsli"

float4x4 g_WorldMatrix;
float4x4 g_ViewMatrix;
float4x4 g_ProjMatrix;
texture2D g_DiffuseTexture;

struct VS_IN
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
    float2 lifetime : TEXCOORD1;
};
struct VS_OUT { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; float alpha : TEXCOORD1; };

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4x4 instanceWorld = float4x4(input.world0, input.world1, input.world2, input.world3);
    float4 position = mul(float4(input.position, 1.f), instanceWorld);
    position = mul(position, g_WorldMatrix);
    output.position = mul(position, mul(g_ViewMatrix, g_ProjMatrix));
    output.texcoord = input.texcoord;
    output.alpha = 1.f - saturate(input.lifetime.y / max(input.lifetime.x, 0.0001f));
    return output;
}

float4 PS_Main(VS_OUT input) : SV_TARGET0
{
    float4 color = g_DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    color.a *= input.alpha;
    if (color.a < 0.01f) discard;
    return color;
}

technique11 DefaultTechnique
{
    pass Default
    {
        SetRasterizerState(RS_Default); SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main(); PixelShader = compile ps_5_0 PS_Main();
    }
}
