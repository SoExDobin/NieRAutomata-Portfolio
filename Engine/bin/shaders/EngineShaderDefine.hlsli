#ifndef ENGINE_SHADER_DEFINE_HLSLI
#define ENGINE_SHADER_DEFINE_HLSLI

RasterizerState RS_Default
{
    FillMode = Solid;
    CullMode = Back;
    FrontCounterClockwise = false;
};

// Deferred point lights: the full-screen quad clipped to the light's screen rectangle (Light::Render sets it).
RasterizerState RS_Scissor
{
    FillMode = Solid;
    CullMode = Back;
    FrontCounterClockwise = false;
    ScissorEnable = true;
};

RasterizerState RS_CullClockWise
{
    FillMode = Solid;
    CullMode = Front;
    FrontCounterClockwise = false;
};

// Two-sided: hair cards, foliage. Flip the normal on back faces with SV_IsFrontFace.
RasterizerState RS_CullNone
{
    FillMode = Solid;
    CullMode = None;
    FrontCounterClockwise = false;
};

// ShadowCompatible passes (cascaded shadow map depth). Two-sided like the map passes. The slope-scaled
// bias keeps lit surfaces from shadowing themselves (acne); no depth clip, so a caster in front of the
// light's near plane is pressed onto it (depth 0) instead of being cut away. Bias values are tuned in S5.
// No constant DepthBias: the baked static map (D16 over the whole level) shares this pass, and there 1000
// units pushed casters about 20 m away from the light, so trees and bushes cast nothing. The compare bias
// and normal offset in Node_CascadeShadow / Node_BakedShadow keep surfaces from shadowing themselves.
RasterizerState RS_ShadowDepth
{
    FillMode = Solid;
    CullMode = None;
    FrontCounterClockwise = false;
    DepthBias = 0;
    SlopeScaledDepthBias = 2.f;
    DepthBiasClamp = 0.f;
    DepthClipEnable = false;
};

DepthStencilState DSS_Default
{
    DepthEnable = true;
    DepthWriteMask = All;
    DepthFunc = Less_Equal;
};

DepthStencilState DSS_None
{
    DepthEnable = false;
    DepthWriteMask = Zero;
};

// Depth test without depth write: decals and overlays drawn on top of already written geometry.
DepthStencilState DSS_TestNoWrite
{
    DepthEnable = true;
    DepthWriteMask = Zero;
    DepthFunc = Less_Equal;
};

BlendState BS_Default
{
    BlendEnable[0] = false;
    BlendEnable[1] = false;
};

BlendState BS_AlphaBlend
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;
    SrcBlend = Src_Alpha;
    DestBlend = Inv_Src_Alpha;
    BlendOp = Add;
    SrcBlendAlpha = One;
    DestBlendAlpha = Inv_Src_Alpha;
    BlendOpAlpha = Add;
};

// Both MRT_LIGHT targets add (shade and specular): a point light drawn after the sun must not replace the
// sun's specular in RT_SPECULAR (metal hair, lit only by specular, turned black inside a lamp's range).
BlendState BS_Additive
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;
    SrcBlend = One;
    DestBlend = One;
    BlendOp = Add;
    SrcBlendAlpha = One;
    DestBlendAlpha = One;
    BlendOpAlpha = Add;
};

// Premultiplied alpha (forward BLEND passes such as water): the shader already multiplied what covers the
// background by alpha and adds reflections on top; the background keeps 1 - alpha. The target alpha stays 1.
BlendState BS_PremultipliedAlpha
{
    BlendEnable[0] = true;
    SrcBlend = One;
    DestBlend = Inv_Src_Alpha;
    BlendOp = Add;
    SrcBlendAlpha = One;
    DestBlendAlpha = Inv_Src_Alpha;
    BlendOpAlpha = Add;
};

// G-buffer overlay: multiplies RT_DIFFUSE (target 0) by the source color and leaves
// RT_NORMAL, RT_MATERIAL and RT_DEPTH (targets 1-3) untouched through a zero write mask.
// Effects11 always sets IndependentBlendEnable (EffectNonRuntime.cpp, SBlendBlock), so [i] states apply per target.
BlendState BS_GBufferMultiply
{
    BlendEnable[0] = true;
    SrcBlend[0] = Dest_Color;
    DestBlend[0] = Zero;
    BlendOp[0] = Add;
    SrcBlendAlpha[0] = Zero;
    DestBlendAlpha[0] = One;
    BlendOpAlpha[0] = Add;
    RenderTargetWriteMask[0] = 0x0F;
    RenderTargetWriteMask[1] = 0x00;
    RenderTargetWriteMask[2] = 0x00;
    RenderTargetWriteMask[3] = 0x00;
};

// G-buffer decal (Node_WriteGBufferAlphaBlend): RT_DIFFUSE, RT_NORMAL and RT_MATERIAL (targets 0-2) blend
// over the surface below by the source alpha and keep their destination alpha; RT_DEPTH (target 3) is
// write-masked so the surface below keeps its depth.
BlendState BS_GBufferAlphaBlend
{
    BlendEnable[0] = true;
    SrcBlend[0] = Src_Alpha;
    DestBlend[0] = Inv_Src_Alpha;
    BlendOp[0] = Add;
    SrcBlendAlpha[0] = Zero;
    DestBlendAlpha[0] = One;
    BlendOpAlpha[0] = Add;
    BlendEnable[1] = true;
    SrcBlend[1] = Src_Alpha;
    DestBlend[1] = Inv_Src_Alpha;
    BlendOp[1] = Add;
    SrcBlendAlpha[1] = Zero;
    DestBlendAlpha[1] = One;
    BlendOpAlpha[1] = Add;
    BlendEnable[2] = true;
    SrcBlend[2] = Src_Alpha;
    DestBlend[2] = Inv_Src_Alpha;
    BlendOp[2] = Add;
    SrcBlendAlpha[2] = Zero;
    DestBlendAlpha[2] = One;
    BlendOpAlpha[2] = Add;
    RenderTargetWriteMask[0] = 0x0F;
    RenderTargetWriteMask[1] = 0x0F;
    RenderTargetWriteMask[2] = 0x0F;
    RenderTargetWriteMask[3] = 0x00;
};

sampler LinearWrapSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
    AddressW = Wrap;
};

sampler LinearClampSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
    AddressW = Clamp;
};

// Depth reads: interpolating depth makes up depths that do not exist at edges, so read it point sampled.
sampler PointClampSampler = sampler_state
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

// Cascaded shadow map: hardware 2x2 comparison per tap; outside the map counts as lit.
SamplerComparisonState ShadowSampler
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Border;
    AddressV = Border;
    BorderColor = float4(1.f, 1.f, 1.f, 1.f);
    ComparisonFunc = LESS_EQUAL;
};

#endif
