#include "EngineShaderDefine.hlsli"

float4x4 g_WorldMatrix;
float4x4 g_ViewMatrix;
float4x4 g_ProjMatrix;
textureCUBE g_Texture;

struct VS_IN { float3 position : POSITION; float3 texcoord : TEXCOORD0; };
struct VS_OUT { float4 position : SV_POSITION; float3 texcoord : TEXCOORD0; };

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = mul(float4(input.position, 1.f), mul(mul(g_WorldMatrix, g_ViewMatrix), g_ProjMatrix));
    output.position.z = output.position.w;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS_Main(VS_OUT input) : SV_TARGET0
{
    return g_Texture.Sample(LinearWrapSampler, input.texcoord);
}

technique11 DefaultTechnique
{
    pass Default
    {
        SetRasterizerState(RS_CullClockWise);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Main();
    }
}
