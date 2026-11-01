#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"

texture2D g_DiffuseTexture;

struct VS_IN { float3 position : POSITION; float3 normal : NORMAL; float2 texcoord : TEXCOORD0; };
struct VS_OUT { float4 position : SV_POSITION; float3 normal : NORMAL; float2 texcoord : TEXCOORD0; };

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = mul(float4(input.position, 1.f), mul(mul(g_WorldMatrix, g_ViewMatrix), g_ProjMatrix));
    output.normal = normalize(mul(float4(input.normal, 0.f), g_WorldMatrix).xyz);
    output.texcoord = input.texcoord;
    return output;
}

PS_GBUFFER_OUT PS_Main(VS_OUT input)
{
    float4 albedo = g_DiffuseTexture.Sample(LinearWrapSampler, input.texcoord);
    if (albedo.a < 0.1f) discard;
    // Terrain has no mask texture: it writes the default surface.
    return Node_WriteGBuffer(Node_DefaultSurface(albedo, input.normal, input.position.z));
}

technique11 DefaultTechnique
{
    pass Default
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Main();
    }
}
