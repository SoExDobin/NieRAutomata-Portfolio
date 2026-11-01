#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"
#include "EngineShaderMaterialTexture.hlsli"
#include "EngineShaderVertex.hlsli"

typedef VTXMESH VS_IN;   // EngineShaderVertex.hlsli
struct VS_OUT { float4 position : SV_POSITION; float3 normal : NORMAL; float2 texcoord : TEXCOORD0; };

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = Node_ObjectToClip(input.position);
    output.normal = Node_ObjectToWorldDirection(input.normal);
    output.texcoord = input.texcoord;
    return output;
}

PS_GBUFFER_OUT PS_Main(VS_OUT input)
{
    float4 albedo = Node_IsBound(g_AlbedoMap)
        ? g_AlbedoMap.Sample(LinearWrapSampler, input.texcoord)
        : float4(1.f, 1.f, 1.f, 1.f);
    if (albedo.a < 0.3f) discard;
    SurfaceData surface = Node_DefaultSurface(albedo, input.normal, input.position.z);
    Node_ApplyNierMask(surface, Node_MaterialMask(input.texcoord).rgb);
    Node_ApplyMaterialParameters(surface);
    return Node_WriteGBuffer(surface);
}

// ShadowCompatible: cascaded shadow map depth (Renderer shadow pass), material alpha clip only.
void PS_ShadowDepth(VS_OUT input)
{
    if (Node_IsBound(g_AlbedoMap))
        Node_ShadowDepthClip(input.texcoord);
}

technique11 DefaultTechnique
{
    pass Default_Pass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main(); PixelShader = compile ps_5_0 PS_Main();
    }
    pass AlphaBlend
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main(); PixelShader = compile ps_5_0 PS_Main();
    }
    pass ShadowCompatible
    {
        SetRasterizerState(RS_ShadowDepth);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_ShadowDepth();
    }
}
