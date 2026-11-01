#include "EngineShaderDefine.hlsli"
#include "EngineShaderMaterial.hlsli"

// ObjectBuffer g_WorldMatrix places the screen quad, ScreenBuffer projects it.
// CameraBuffer is the scene camera of the view being lit, LightBuffer the light being drawn.

texture2D g_Texture;
texture2D g_DiffuseTexture;
texture2D g_NormalTexture;
texture2D g_ShadeTexture;
texture2D g_MaterialTexture;
texture2D g_DepthTexture;
texture2D g_SpecularTexture;
texture2D g_AOTexture;              // RT_AO, SSAO after blur
// g_ShadowTexture, the shadow nodes and PointClampSampler/ShadowSampler come from the engine hlsli.

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

float4 PS_Debug(VS_OUT input) : SV_TARGET0
{
    return g_Texture.Sample(LinearClampSampler, input.texcoord);
}

// MRT_LIGHT layout: RT_SHADE, RT_SPECULAR.
struct PS_LIGHT_OUT
{
    float4 shade : SV_TARGET0;
    float4 specular : SV_TARGET1;
};

PS_LIGHT_OUT PS_Directional(VS_OUT input)
{
    PS_LIGHT_OUT output;
    float3 encodedNormal = g_NormalTexture.Sample(LinearClampSampler, input.texcoord).xyz;
    float3 normal = normalize(encodedNormal * 2.f - 1.f);
    float4 material = g_MaterialTexture.Sample(LinearClampSampler, input.texcoord);
    float depth = g_DepthTexture.Sample(LinearClampSampler, input.texcoord).r;
    float3 albedo = g_DiffuseTexture.Sample(LinearClampSampler, input.texcoord).rgb;
    float3 worldPosition = Node_ReconstructWorldPosition(input.texcoord, depth, g_ProjInverseMatrix, g_ViewInverseMatrix);
    // SSAO는 해가 아닌 빛(환경광, 환경 반사)만 줄인다.
    const float ao = g_SSAOParams.w > 0.f ? g_AOTexture.Sample(LinearClampSampler, input.texcoord).r : 1.f;
    float3 shade, specular;
    Node_DirectionalLight(normal, worldPosition, albedo, material.r, material.g, ao, shade, specular);
    output.shade = float4(shade, 1.f);
    output.specular = float4(specular, 1.f);

    // Ctrl+F8 Cascade Colors: 캐스케이드 0~3을 빨강·초록·파랑·노랑으로 물들인다. 그림자 거리 밖은 그대로 둔다.
    if (g_ShadowDebug.x > 0.f)
    {
        const float3 cascadeTints[5] = { float3(1.f, 0.35f, 0.35f), float3(0.35f, 1.f, 0.35f),
            float3(0.35f, 0.35f, 1.f), float3(1.f, 1.f, 0.35f), float3(1.f, 1.f, 1.f) };
        output.shade.rgb *= cascadeTints[Node_ShadowCascade(mul(float4(worldPosition, 1.f), g_ViewMatrix).z)];
    }
    return output;
}

// One point light (lamp, neon) added over the whole screen: diffuse and GGX specular, no ambient, no shadow.
// The falloff is inverse-square in units of the range (POINT_FALLOFF: half the color at 0.58 of the range),
// windowed to reach 0 at g_LightRange, so the color is the brightness at the light and a wider range lights a
// wider pool, as the original area lights (GAD m_distance) do. Pixels past the range are skipped.
static const float POINT_FALLOFF = 3.f;

float Node_PointAttenuation(float distance)
{
    const float ratio = distance / g_LightRange;
    const float window = saturate(1.f - ratio * ratio * ratio * ratio);
    return window * window / (1.f + POINT_FALLOFF * ratio * ratio);
}

// A spot light's cone around g_LightDir: 1 inside the inner cone, easing to 0 at the outer one (g_LightSpot.xy are
// their cosines). fromLight is normalized.
float Node_SpotCone(float3 fromLight)
{
    return smoothstep(g_LightSpot.x, g_LightSpot.y, dot(fromLight, normalize(g_LightDir.xyz)));
}

// Diffuse and GGX specular of the point or spot light on this pixel's surface; 0 past the range. Metals (A2's hair, mask
// 0.97) take no diffuse, so with the GGX highlight alone they read black inside a lit pool; as under the sun
// (Node_DirectionalLight) they also reflect the surroundings, here as bright as this light makes the pixel.
PS_LIGHT_OUT Node_LocalLight(float2 texcoord, float3 worldPosition, bool spot)
{
    PS_LIGHT_OUT output;
    output.shade = float4(0.f, 0.f, 0.f, 1.f);
    output.specular = float4(0.f, 0.f, 0.f, 1.f);
    const float3 toLightRaw = g_LightPosition.xyz - worldPosition;
    const float distance = length(toLightRaw);
    if (distance < g_LightRange)
    {
        const float3 normal = normalize(g_NormalTexture.Sample(PointClampSampler, texcoord).xyz * 2.f - 1.f);
        const float4 material = g_MaterialTexture.Sample(PointClampSampler, texcoord);
        const float3 albedo = g_DiffuseTexture.Sample(PointClampSampler, texcoord).rgb;
        const float3 toLight = toLightRaw / max(distance, 1e-4f);
        const float3 toEye = normalize(g_CameraPosition.xyz - worldPosition);
        const float attenuation = Node_PointAttenuation(distance) * (spot ? Node_SpotCone(-toLight) : 1.f);

        output.shade.rgb = g_LightDiffuse.rgb * saturate(dot(normal, toLight)) * attenuation;
        output.specular.rgb = Node_SpecularGGX(normal, toLight, toEye, albedo, material.r, material.g)
            * g_LightSpecular.rgb * attenuation;
        output.specular.rgb += Node_EnvironmentSpecular(normal, toEye, albedo, material.r, material.g,
            float3(0.f, 0.f, 0.f), g_LightDiffuse.rgb * attenuation);
    }
    return output;
}

PS_LIGHT_OUT PS_Point(VS_OUT input)
{
    const float depth = g_DepthTexture.Sample(PointClampSampler, input.texcoord).r;
    clip(0.99999f - depth);     // sky
    const float3 worldPosition = Node_ReconstructWorldPosition(input.texcoord, depth, g_ProjInverseMatrix, g_ViewInverseMatrix);
    clip(g_LightRange - length(g_LightPosition.xyz - worldPosition));
    return Node_LocalLight(input.texcoord, worldPosition, false);
}

static const uint SPOT_BEAM_STEPS = 24;
static const float SPOT_BEAM_PHASE_G = 0.3f;

// One spot light (a stage spot): the point light narrowed to its cone, and with g_LightSpot.z > 0 its beam, the light
// the air inside the cone scatters toward the eye along this pixel's view ray up to the surface: the sum of cone *
// falloff * Henyey-Greenstein phase over the steps, times the scattering and the step. Only the part of the ray inside
// the range sphere and in front of the light is marched, from a start jittered per pixel (interleaved gradient noise,
// as the sun's volumetric). The beam is light reaching the eye rather than the surface, so it goes to the specular
// target, which the composite adds without the albedo. It has no shadow: it passes through whatever stands in the cone.
PS_LIGHT_OUT PS_Spot(VS_OUT input)
{
    const float depth = g_DepthTexture.Sample(PointClampSampler, input.texcoord).r;
    clip(0.99999f - depth);     // sky: the composite keeps the sky box there
    const float3 worldPosition = Node_ReconstructWorldPosition(input.texcoord, depth, g_ProjInverseMatrix, g_ViewInverseMatrix);
    if (g_LightSpot.z <= 0.f)
        clip(g_LightRange - length(g_LightPosition.xyz - worldPosition));
    PS_LIGHT_OUT output = Node_LocalLight(input.texcoord, worldPosition, true);
    if (g_LightSpot.z <= 0.f)
        return output;

    const float3 toSurface = worldPosition - g_CameraPosition.xyz;
    const float surfaceDistance = length(toSurface);
    const float3 direction = toSurface / max(surfaceDistance, 1e-4f);
    const float3 origin = g_CameraPosition.xyz - g_LightPosition.xyz;
    // Range sphere: |origin + t direction| = range.
    const float b = dot(origin, direction);
    const float h = b * b - (dot(origin, origin) - g_LightRange * g_LightRange);
    if (h <= 0.f)
        return output;
    float start = max(-b - sqrt(h), 0.f);
    float end = min(-b + sqrt(h), surfaceDistance);
    // In front of the light: dot(origin + t direction, lightDir) >= 0.
    const float3 lightDir = normalize(g_LightDir.xyz);
    const float along = dot(origin, lightDir);
    const float rate = dot(direction, lightDir);
    if (abs(rate) > 1e-4f)
    {
        if (rate > 0.f)
            start = max(start, -along / rate);
        else
            end = min(end, -along / rate);
    }
    else if (along < 0.f)
        end = start;
    if (end <= start)
        return output;

    const float stepLength = (end - start) / SPOT_BEAM_STEPS;
    const float noise = frac(52.9829189f * frac(dot(input.position.xy, float2(0.06711056f, 0.00583715f))));
    const float g = SPOT_BEAM_PHASE_G;
    float scattering = 0.f;
    [loop] for (uint i = 0; i < SPOT_BEAM_STEPS; ++i)
    {
        const float3 fromLight = origin + direction * (start + (i + noise) * stepLength);
        const float distance = length(fromLight);
        const float3 ray = fromLight / max(distance, 1e-4f);
        // Light travelling along ray turned toward the eye (-direction).
        const float phase = (1.f - g * g) / pow(max(1.f + g * g - 2.f * g * dot(ray, -direction), 1e-4f), 1.5f);
        scattering += Node_PointAttenuation(distance) * Node_SpotCone(ray) * phase;
    }
    output.specular.rgb += g_LightDiffuse.rgb * g_LightSpot.z * scattering * stepLength;
    return output;
}

float4 PS_Combined(VS_OUT input) : SV_TARGET0
{
    float4 diffuse = g_DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    float4 shade = g_ShadeTexture.Sample(LinearClampSampler, input.texcoord);
    float4 specular = g_SpecularTexture.Sample(LinearClampSampler, input.texcoord);
    float4 material = g_MaterialTexture.Sample(LinearClampSampler, input.texcoord);
    // The G-buffer diffuse target clears to zero, so alpha 0 means no geometry was written
    // here. This pass runs with no depth test and no blending, so without the clip it would
    // overwrite whatever the PRIORITY group already drew - the sky box - with black.
    clip(diffuse.a - 0.0001f);
    return float4(Node_ToneMap(Node_CombineLighting(diffuse.rgb, material.r, material.b, shade.rgb, specular.rgb)), diffuse.a);
}

static const uint SSAO_SAMPLE_COUNT = 12;

// RT_DEPTH(clip z/w) -> View 공간 깊이. 원근 투영: depth = _33 + _43 / z
float Node_ViewDepth(float depth)
{
    return g_ProjMatrix._43 / (depth - g_ProjMatrix._33);
}

float PS_SSAO(VS_OUT input) : SV_TARGET0
{
    const float depth = g_DepthTexture.Sample(PointClampSampler, input.texcoord).r;
    if (depth >= 1.f)
        return 1.f;     // 하늘

    const float radius = g_SSAOParams.x;
    const float intensity = g_SSAOParams.y;
    const float bias = g_SSAOParams.z;

    const float3 normal = normalize(g_NormalTexture.Sample(PointClampSampler, input.texcoord).xyz * 2.f - 1.f);
    const float3 position = Node_ReconstructWorldPosition(input.texcoord, depth, g_ProjInverseMatrix, g_ViewInverseMatrix);
    const float centerZ = Node_ViewDepth(depth);

    // 법선을 z축으로 하는 좌표계
    const float3 up = abs(normal.y) < 0.99f ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
    const float3 tangent = normalize(cross(up, normal));
    const float3 bitangent = cross(normal, tangent);

    // 픽셀마다 샘플을 다르게 돌린다. 생기는 노이즈는 PS_SSAO_Blur가 지운다.
    const float rotation = frac(52.9829189f * frac(dot(input.position.xy, float2(0.06711056f, 0.00583715f)))) * 6.2831853f;

    float occlusion = 0.f;
    [unroll] for (uint i = 0; i < SSAO_SAMPLE_COUNT; ++i)
    {
        // 반구 위 나선(황금각)으로 퍼뜨리고, 가까운 샘플을 더 촘촘히 둔다.
        const float t = (i + 0.5f) / SSAO_SAMPLE_COUNT;
        const float phi = i * 2.39996323f + rotation;
        const float r = sqrt(t);
        const float3 direction = tangent * (cos(phi) * r) + bitangent * (sin(phi) * r) + normal * sqrt(1.f - t);
        const float3 samplePosition = position + direction * radius * lerp(0.1f, 1.f, t * t);

        // 샘플을 화면으로 투영 (clip.w = 샘플의 View 깊이)
        const float4 clip = mul(mul(float4(samplePosition, 1.f), g_ViewMatrix), g_ProjMatrix);
        const float2 sampleUV = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
        if (any(sampleUV < 0.f) || any(sampleUV > 1.f))
            continue;

        // 그 자리 표면이 샘플보다 앞에 있으면 가려진 것. 멀리 떨어진 물체는 range로 덜 센다.
        const float sceneZ = Node_ViewDepth(g_DepthTexture.SampleLevel(PointClampSampler, sampleUV, 0).r);
        const float range = saturate(radius / abs(centerZ - sceneZ));
        occlusion += (sceneZ < clip.w - bias ? 1.f : 0.f) * range;
    }
    return saturate(1.f - intensity * occlusion / SSAO_SAMPLE_COUNT);
}

// Ctrl+F1 미리보기: 그림자 맵 캐스케이드 0 슬라이스의 광원 깊이(가까울수록 어둡다).
float4 PS_ShadowDebug(VS_OUT input) : SV_TARGET0
{
    const float depth = g_BakedShadowTexture.SampleLevel(LinearClampSampler, input.texcoord, 0).r;
    return float4(depth, depth, depth, 1.f);
}

// 4x4 평균으로 회전 무늬를 지운다. 깊이가 5% 넘게 다른 픽셀은 다른 물체로 보고 빼서 경계가 번지지 않게 한다.
float PS_SSAO_Blur(VS_OUT input) : SV_TARGET0
{
    const float depth = g_DepthTexture.Sample(PointClampSampler, input.texcoord).r;
    if (depth >= 1.f)
        return 1.f;
    const float centerZ = Node_ViewDepth(depth);

    float sum = 0.f;
    float weightSum = 0.f;
    [unroll] for (int y = -2; y < 2; ++y)
        [unroll] for (int x = -2; x < 2; ++x)
        {
            const float2 uv = input.texcoord + float2(x, y) * g_ScreenTexel.xy;
            const float z = Node_ViewDepth(g_DepthTexture.Sample(PointClampSampler, uv).r);
            const float weight = abs(z - centerZ) <= 0.05f * centerZ ? 1.f : 0.f;
            sum += g_Texture.Sample(PointClampSampler, uv).r * weight;
            weightSum += weight;
        }
    return weightSum > 0.f ? sum / weightSum : 1.f;
}

// 가림 컬링: 줄인 깊이 텍셀 하나가 덮는 8×8(OCCLUSION_DEPTH_BLOCK) View 픽셀 중 가장 먼 깊이.
// 화면 밖 Load는 0이라 최댓값에 끼지 않는다.
float PS_DepthReduce(VS_OUT input) : SV_TARGET0
{
    const int2 origin = int2(input.position.xy) * 8;
    float depth = 0.f;
    [unroll] for (int y = 0; y < 8; ++y)
        [unroll] for (int x = 0; x < 8; ++x)
            depth = max(depth, g_DepthTexture.Load(int3(origin + int2(x, y), 0)).r);
    return depth;
}

technique11 DefaultTechnique
{
    pass Debug
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Debug();
    }

    pass Directional
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Directional();
    }

    pass Point
    {
        SetRasterizerState(RS_Scissor);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Point();
    }

    pass Combined
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Combined();
    }

    pass SSAO
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_SSAO();
    }

    pass SSAO_Blur
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_SSAO_Blur();
    }

    pass ShadowDebug
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_ShadowDebug();
    }

    pass DepthReduce
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_DepthReduce();
    }

    pass Spot
    {
        SetRasterizerState(RS_Scissor);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_Spot();
    }
}
