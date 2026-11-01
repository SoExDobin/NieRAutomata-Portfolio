#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"
#include "EngineShaderMaterialTexture.hlsli"
#include "EngineShaderVertex.hlsli"

// Weapon shader: wp0070 (WP_0070_Body), wp0220 (WP_0220_Body), wp3000 (WP3000_Body, WP3000_weapon).
// The loader registers "Shader_VtxAnimMesh_Weapon.hlsl" as VTX_Animation_Mesh_Weapon (VTXANIMMESH layout),
// the weapon and sheath objects create their shader with that tag, and the presets
// (ProjectSetting/Material/wp*.json) name it in "shaderTag" so Model Viewer validates against these passes.
// Every material value comes from the preset "parameters" through MaterialBuffer.

// The optional PBS00 detail normal is the engine slot g_DetailNormalMap (the weapons' .blend has
// g_UseDetailNormalMap 0, so presets leave it unbound).

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

// WP_PBS - NieR PBS00_XXXXX "Default" as the .blend builds it (wp0220.blend):
//   Albedo x MaskMap.B -> Base Color, MaskMap -> Invert Green -> R Metallic, G Roughness
//   NormalMap (+ DetailNormalMap x MaskMap.A when a detail slot is bound) -> Normal
//   Alpha 1 (g_1BitMask 0) -> preset opacityMaskClipValue 0. Mask color space sRGB -> preset maskSrgb.
PS_GBUFFER_OUT PS_Weapon(VS_OUT input)
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

// ShadowCompatible: cascaded shadow map depth (Renderer shadow pass), material alpha clip only.
void PS_ShadowDepth(VS_OUT input)
{
    Node_ShadowDepthClip(input.texcoord);
}

technique11 DefaultTechnique
{
    pass WP_PBS
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Weapon();
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
