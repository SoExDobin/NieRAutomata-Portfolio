#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"

// Screen post-processing after the deferred composite (Renderer, POSTPROCESS passes).
// ObjectBuffer g_WorldMatrix places the screen quad, ScreenBuffer projects it, CameraBuffer is the view's camera,
// LightBuffer the active directional light (Renderer::Bind_ForwardLighting, with the cascaded shadow and RT_DEPTH),
// PostProcessBuffer the grading and volumetric values.

texture2D g_Texture;                // pass input: RT_VOLUMETRIC_RAW (blur) or the RT_SCENE screen copy (grade)
texture2D g_DepthTexture;           // RT_DEPTH, opaque scene depth
texture2D g_VolumetricTexture;      // RT_VOLUMETRIC, sun in-scattering after blur
texture2D g_BloomTexture;           // RT_BLOOM, the bright parts of the screen after blur
texture2D g_EmissiveTexture;        // RT_EMISSIVE, RENDERGROUP::EMISSIVE glow (HDR), read when g_Glow.x is set

struct VS_IN
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = mul(float4(input.position, 1.f), mul(g_WorldMatrix, g_ScreenProjMatrix));
    output.texcoord = input.texcoord;
    return output;
}

static const uint VOLUMETRIC_STEPS = 16;

// Volumetric light (Ctrl+F4). Sun light scattered toward the eye along this pixel's view ray, up to the opaque surface
// or the march distance: sum of density * step * transmittance * shadow, times the Henyey-Greenstein phase
// (1 on average, larger toward the sun). Each step reads the baked static shadow once (buildings and terrain cut
// the shafts; characters do not). The start is jittered per pixel (interleaved gradient noise); PS_Volumetric_Blur
// removes the pattern.
float PS_Volumetric(VS_OUT input) : SV_TARGET0
{
    const float depth = g_DepthTexture.Sample(PointClampSampler, input.texcoord).r;
    const float3 target = Node_ReconstructWorldPosition(input.texcoord, min(depth, 0.999999f), g_ProjInverseMatrix, g_ViewInverseMatrix);
    const float3 toTarget = target - g_CameraPosition.xyz;
    const float targetDistance = length(toTarget);
    const float3 direction = toTarget / max(targetDistance, 1e-4f);
    const float rayLength = depth >= 1.f ? g_Volumetric.z : min(targetDistance, g_Volumetric.z);
    const float stepLength = rayLength / VOLUMETRIC_STEPS;
    const float density = g_Volumetric.y;

    const float g = g_VolumetricPhase.x;
    const float cosTheta = dot(direction, -normalize(g_LightDir.xyz));
    const float phase = (1.f - g * g) / pow(max(1.f + g * g - 2.f * g * cosTheta, 1e-4f), 1.5f);

    const float noise = frac(52.9829189f * frac(dot(input.position.xy, float2(0.06711056f, 0.00583715f))));
    const bool shadowed = g_ShadowParams.x > 0.f && g_BakedShadowParams.x > 0.f;
    float scattering = 0.f;
    [loop] for (uint i = 0; i < VOLUMETRIC_STEPS; ++i)
    {
        const float s = (i + noise) * stepLength;
        const float3 position = g_CameraPosition.xyz + direction * s;
        const float lit = shadowed ? Node_BakedShadowTap(position) : 1.f;
        scattering += lit * exp(-density * s);
    }
    return scattering * density * stepLength * phase;
}

// 4x4 average removes the jitter pattern. No depth weighting: the shafts are soft and the haze is smooth.
float PS_Volumetric_Blur(VS_OUT input) : SV_TARGET0
{
    float sum = 0.f;
    [unroll] for (int y = -2; y < 2; ++y)
        [unroll] for (int x = -2; x < 2; ++x)
            sum += g_Texture.Sample(PointClampSampler, input.texcoord + (float2(x, y) + 0.5f) * g_ScreenTexel.xy).r;
    return sum / 16.f;
}

// The level fog (DirectionalLight Fog*): every pixel fades toward the fog color over its distance up to the max
// opacity, and the sky, infinitely far, takes the max opacity, so far buildings and the clouds sink into one tinted
// haze (the amusement park's dusk). Looking toward the sun the fog warms by the sun glow color (a broad lobe, as in
// the original FogLUT's sun spot), so a low sun makes the glowing horizon behind the castle.
float3 Node_ApplyFog(float3 color, float2 uv)
{
    if (g_Fog.w <= 0.f)
        return color;
    const float depth = g_DepthTexture.Sample(PointClampSampler, uv).r;
    const float3 worldPosition = Node_ReconstructWorldPosition(uv, min(depth, 0.999999f), g_ProjInverseMatrix, g_ViewInverseMatrix);
    const float3 toPixel = worldPosition - g_CameraPosition.xyz;
    const float distance = length(toPixel);
    const float fog = depth < 1.f ? g_Fog.w * (1.f - exp(-distance / g_FogParams.x)) : g_Fog.w;
    const float sunGlow = pow(saturate(dot(toPixel / max(distance, 1e-4f), -normalize(g_LightDir.xyz))), 6.f);
    return lerp(color, g_Fog.rgb + g_FogParams.yzw * sunGlow, fog);
}

// Bloom: the bright parts of the fogged screen copy (luminance above BLOOM_THRESHOLD, a soft ramp) blurred by a
// 13-tap Gaussian across 4 px steps, horizontally then vertically (about 50 px each way), so lamps, lit windows and
// the glowing sky bleed softly over their edges. The source is fogged like PS_Grade, so cloud highlights the fog
// hides do not glow through it. The prefilter scales it by g_ColorGrade.w (the level's bloom) and adds the glow
// (RT_EMISSIVE x g_Glow.y) with no threshold; PS_Grade adds the result.
static const float BLOOM_THRESHOLD = 0.55f;
static const float BLOOM_STEP = 4.f;
static const float BLOOM_WEIGHTS[7] = { 0.1585f, 0.1465f, 0.1157f, 0.0781f, 0.0450f, 0.0222f, 0.0093f };

float3 Node_BloomTap(float2 uv, bool prefilter)
{
    const float3 color = g_Texture.Sample(LinearClampSampler, uv).rgb;
    if (!prefilter)
        return color;
    const float3 fogged = Node_ApplyFog(color, uv);
    const float luma = dot(fogged, float3(0.2126f, 0.7152f, 0.0722f));
    float3 bright = fogged * smoothstep(BLOOM_THRESHOLD, 1.f, luma) * g_ColorGrade.w;
    // Glow (machine eyes) blooms whatever its colour: a red eye is too dark in luma to pass the threshold.
    if (g_Glow.x > 0.f)
        bright += g_EmissiveTexture.Sample(LinearClampSampler, uv).rgb * g_Glow.y;
    return bright;
}

float4 PS_Bloom_H(VS_OUT input) : SV_TARGET0
{
    float3 sum = Node_BloomTap(input.texcoord, true) * BLOOM_WEIGHTS[0];
    [unroll] for (int i = 1; i < 7; ++i)
    {
        const float2 offset = float2(i * BLOOM_STEP * g_ScreenTexel.x, 0.f);
        sum += (Node_BloomTap(input.texcoord + offset, true) + Node_BloomTap(input.texcoord - offset, true)) * BLOOM_WEIGHTS[i];
    }
    return float4(sum, 1.f);
}

float4 PS_Bloom_V(VS_OUT input) : SV_TARGET0
{
    float3 sum = Node_BloomTap(input.texcoord, false) * BLOOM_WEIGHTS[0];
    [unroll] for (int i = 1; i < 7; ++i)
    {
        const float2 offset = float2(0.f, i * BLOOM_STEP * g_ScreenTexel.y);
        sum += (Node_BloomTap(input.texcoord + offset, false) + Node_BloomTap(input.texcoord - offset, false)) * BLOOM_WEIGHTS[i];
    }
    return float4(sum, 1.f);
}

// Screen glitch (Renderer::Start_ScreenGlitch), both strengths fading to 0: by g_Glow.z red and blue are read up to
// GLITCH_SPLIT apart sideways in opposite directions (the colour split alone for a perfect dodge or a shockwave),
// by g_Glow.w (a hit) of GLITCH_BANDS rows the more the stronger also slip sideways by up to GLITCH_SLIP, both
// re-picked GLITCH_RATE times a second (g_Time). The bloom and glow split with the scene, so the bright sparks part
// widest. Tuned on screen.
static const float GLITCH_SPLIT = 0.006f;
static const float GLITCH_SLIP = 0.03f;
static const float GLITCH_BANDS = 36.f;
static const float GLITCH_RATE = 24.f;

float Node_GlitchHash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// The fogged screen at uv with its bloom and glow. The bloom already carries the level's bloom intensity
// (Node_BloomTap); the glow is added unfogged on top.
float3 Node_GradeSource(float2 uv)
{
    float3 color = Node_ApplyFog(g_Texture.Sample(PointClampSampler, uv).rgb, uv);
    if (g_ColorGrade.w > 0.f || g_Glow.x > 0.f)
        color += g_BloomTexture.Sample(LinearClampSampler, uv).rgb;
    if (g_Glow.x > 0.f)
        color += g_EmissiveTexture.Sample(PointClampSampler, uv).rgb;
    return color;
}

// Screen grading after BLEND/DISTORTION: g_Texture is the RT_SCENE copy of the finished screen, sky and effects
// included, so an area darkening (sewer) or a level's look (bright day, night) covers everything but the UI.
// With the volumetric light on, opaque geometry first fades into the ambient-colored haze over its distance
// (capped at the march distance, so the far city stays readable), then the shadowed sun light scattered toward
// the eye is added. The sky keeps its own color and gets only the scattering, so light shafts show against it.
// The level fog (Node_ApplyFog) comes first, then the bloom, read through the glitch's offsets while one runs.
float4 PS_Grade(VS_OUT input) : SV_TARGET0
{
    float3 color;
    if (g_Glow.z > 0.f)
    {
        const float tick = frac(floor(g_Time.x * GLITCH_RATE) / 4096.f) * 4096.f;
        float2 uv = input.texcoord;
        if (g_Glow.w > 0.f)
        {
            const float band = floor(uv.y * GLITCH_BANDS);
            const float slips = step(1.f - 0.5f * g_Glow.w, Node_GlitchHash(float2(band, tick)));
            uv.x += (Node_GlitchHash(float2(tick, band)) * 2.f - 1.f) * GLITCH_SLIP * g_Glow.w * slips;
        }
        const float2 split = float2(GLITCH_SPLIT * g_Glow.z * (0.5f + Node_GlitchHash(float2(tick, -1.f))), 0.f);
        color = float3(Node_GradeSource(uv + split).r, Node_GradeSource(uv).g, Node_GradeSource(uv - split).b);
    }
    else
        color = Node_GradeSource(input.texcoord);
    if (g_Volumetric.w > 0.f)
    {
        const float depth = g_DepthTexture.Sample(PointClampSampler, input.texcoord).r;
        if (depth < 1.f)
        {
            const float3 worldPosition = Node_ReconstructWorldPosition(input.texcoord, depth, g_ProjInverseMatrix, g_ViewInverseMatrix);
            const float distance = min(length(worldPosition - g_CameraPosition.xyz), g_Volumetric.z);
            const float transmittance = exp(-g_Volumetric.y * distance);
            color = color * transmittance + g_LightAmbient.rgb * (1.f - transmittance);
        }
        color += g_LightDiffuse.rgb * g_Volumetric.x * g_VolumetricTexture.Sample(LinearClampSampler, input.texcoord).r;
    }
    color *= g_ColorGrade.x;
    color = (color - 0.5f) * g_ColorGrade.y + 0.5f;
    const float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(luma.xxx, color, g_ColorGrade.z) * g_ColorTint.rgb;
    return float4(saturate(color), 1.f);
}

// Pass order is POSTPROCESS in Engine_Enum.h.
technique11 DefaultTechnique
{
    pass Volumetric
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Volumetric();
    }

    pass Volumetric_Blur
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Volumetric_Blur();
    }

    pass Grade
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Grade();
    }

    pass Bloom_H
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Bloom_H();
    }

    pass Bloom_V
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Bloom_V();
    }
}
