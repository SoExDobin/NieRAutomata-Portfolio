#include "EngineShaderDefine.hlsli"

float4x4 g_WorldMatrix;
float4x4 g_ViewMatrix;
float4x4 g_ProjMatrix;
float4 g_CameraPosition;
uint g_LockUpRight;
texture2D g_DiffuseTexture;

struct VS_IN
{
    float3 position : POSITION;
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
    float2 lifetime : TEXCOORD0;
    uint instanceId : SV_InstanceID;
};
struct VS_OUT { float4 position : POSITION; float2 lifetime : TEXCOORD0; float2 size : TEXCOORD1; };
struct GS_OUT { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; float alpha : TEXCOORD1; };

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4x4 instanceWorld = float4x4(input.world0, input.world1, input.world2, input.world3);
    output.position = mul(mul(float4(input.position, 1.f), instanceWorld), g_WorldMatrix);
    output.lifetime = input.lifetime;
    output.size = float2(length(input.world0.xyz), length(input.world1.xyz));
    return output;
}

[maxvertexcount(4)]
void GS_Main(point VS_OUT input[1], inout TriangleStream<GS_OUT> stream)
{
    float3 center = input[0].position.xyz;
    float3 look = normalize(g_CameraPosition.xyz - center);
    float3 up = (g_LockUpRight != 0) ? float3(0.f, 1.f, 0.f) : normalize(float3(g_ViewMatrix._12, g_ViewMatrix._22, g_ViewMatrix._32));
    float3 right = normalize(cross(up, look));
    up = normalize(cross(look, right));
    float2 corners[4] = { float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(-0.5f, -0.5f), float2(0.5f, -0.5f) };
    float2 uvs[4] = { float2(0.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(1.f, 1.f) };
    float alpha = 1.f - saturate(input[0].lifetime.y / max(input[0].lifetime.x, 0.0001f));
    [unroll] for (uint i = 0; i < 4; ++i)
    {
        GS_OUT output;
        float3 worldPosition = center
                             + right * corners[i].x * input[0].size.x
                             + up * corners[i].y * input[0].size.y;
        output.position = mul(float4(worldPosition, 1.f), mul(g_ViewMatrix, g_ProjMatrix));
        output.texcoord = uvs[i];
        output.alpha = alpha;
        stream.Append(output);
    }
}

float4 PS_Main(GS_OUT input) : SV_TARGET0
{
    float4 color = g_DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    color.a *= input.alpha;
    if (color.a < 0.01f) discard;
    return color;
}

// ExplodeEffect: the current frame of an explosion sequence, faded out over its smoke tail.
float g_FadeAlpha = 1.f;
// rgb multiplies the frame colour (the shockwave dust is lifted to a pale grey), a scales its coverage.
float4 g_ExplodeTint = float4(1.f, 1.f, 1.f, 1.f);

float4 PS_Explode(GS_OUT input) : SV_TARGET0
{
    float4 color = g_DiffuseTexture.Sample(LinearClampSampler, input.texcoord) * g_ExplodeTint;
    color.a *= input.alpha * g_FadeAlpha;
    if (color.a < 0.01f) discard;
    return color;
}

// One quad spanned by the world matrix's right/up rows, so the caller picks its facing: the Pod's muzzle sigil faces the
// shot, its flash has a random roll, a hit spark stands along the way it flies. rgb tints, a scales coverage (.est MOVE rgba).
float4 g_QuadTint = float4(1.f, 1.f, 1.f, 1.f);
// 1 draws a soft round glow instead of the texture (the hot core of a hit spark).
uint g_QuadGlow = 0;
// A spark burning out: x 0~1 how far it has broken up, y its seed (0 0: whole). Noise cells eat it from the tail
// (texcoord y 1) toward the head, each fresh edge flaring white-hot before it goes.
float2 g_QuadDissolve = float2(0.f, 0.f);

float Node_Hash(float n)
{
    return frac(sin(n * 12.9898f + 78.233f) * 43758.5453f);
}

float Node_ValueNoise(float2 p)
{
    const float2 i = floor(p);
    const float2 f = frac(p);
    const float2 u = f * f * (3.f - 2.f * f);
    const float n = dot(i, float2(1.f, 57.f));
    return lerp(lerp(Node_Hash(n), Node_Hash(n + 1.f), u.x), lerp(Node_Hash(n + 57.f), Node_Hash(n + 58.f), u.x), u.y);
}

[maxvertexcount(4)]
void GS_Quad(point VS_OUT input[1], inout TriangleStream<GS_OUT> stream)
{
    const float3 center = input[0].position.xyz;
    const float3 right = g_WorldMatrix._11_12_13;
    const float3 up = g_WorldMatrix._21_22_23;
    const float2 corners[4] = { float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(-0.5f, -0.5f), float2(0.5f, -0.5f) };
    const float2 uvs[4] = { float2(0.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(1.f, 1.f) };
    [unroll] for (uint i = 0; i < 4; ++i)
    {
        GS_OUT output;
        output.position = mul(float4(center + right * corners[i].x + up * corners[i].y, 1.f), mul(g_ViewMatrix, g_ProjMatrix));
        output.texcoord = uvs[i];
        output.alpha = 1.f;
        stream.Append(output);
    }
}

float4 PS_Quad(GS_OUT input) : SV_TARGET0
{
    float4 texel = g_DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    if (g_QuadGlow != 0)
    {
        const float2 d = input.texcoord * 2.f - 1.f;
        texel = float4(1.f, 1.f, 1.f, exp(-dot(d, d) * 5.f));
    }
    float coverage = saturate(texel.a * g_QuadTint.a) * g_FadeAlpha;
    float flare = 1.f;
    if (g_QuadDissolve.x > 0.f)
    {
        // Cells are finer along the streak than across it, so it breaks into short bits rather than strips.
        const float noise = Node_ValueNoise(input.texcoord * float2(3.f, 14.f) + g_QuadDissolve.y * 7.31f);
        const float remain = noise * 0.75f + (1.f - input.texcoord.y) * 0.25f - g_QuadDissolve.x;
        clip(remain);
        const float edge = 1.f - smoothstep(0.f, 0.08f, remain);
        flare += edge * 2.f;
        coverage = saturate(coverage * (1.f + edge));
    }
    clip(coverage - 0.004f);
    return float4(texel.rgb * g_QuadTint.rgb * coverage * flare, coverage);
}

// Pl0000Shockwave: a flat ring on the floor (a GS_Quad laid down) that runs out to the blast's reach and dies, bending
// the lit screen across its crest like a lens and glowing pale on it. x progress 0~1, y refraction, z emissive.
Texture2D g_SceneTexture;   // lit screen copied before RENDERGROUP::DISTORTION
float4 g_ShockRing = float4(0.f, 1.f, 1.f, 0.f);

float4 PS_ShockRing(GS_OUT input) : SV_TARGET0
{
    const float2 fromCenter = input.texcoord * 2.f - 1.f;
    const float d = length(fromCenter);
    const float p = saturate(g_ShockRing.x);
    const float radius = 1.f - pow(1.f - p, 3.f);
    const float width = lerp(0.05f, 0.16f, p);
    const float x = (d - radius) / width;
    const float band = exp(-x * x);
    const float fade = (1.f - p) * (1.f - p);
    const float strength = band * fade;
    clip(strength - 0.01f);

    // The quad's outward direction carried to the screen through the texcoord derivatives (pixels per texcoord).
    const float2 du = ddx(input.texcoord);
    const float2 dv = ddy(input.texcoord);
    const float2 outward = fromCenter / max(d, 0.0001f);
    const float det = du.x * dv.y - dv.x * du.y;
    float2 screenOut = float2(dv.y * outward.x - dv.x * outward.y, -du.y * outward.x + du.x * outward.y) / (abs(det) > 1e-12f ? det : 1e-12f);
    screenOut /= max(length(screenOut), 0.0001f);

    // Inside the crest pulls in and outside pushes out; the shift is a fraction of the screen height.
    uint w, h;
    g_SceneTexture.GetDimensions(w, h);
    const float2 shift = screenOut * (-x * band) * fade * g_ShockRing.y * (0.025f * h);
    float3 scene = g_SceneTexture.Sample(LinearClampSampler, (input.position.xy + shift) / float2(w, h)).rgb;
    scene += float3(1.f, 0.96f, 0.88f) * band * band * fade * g_ShockRing.z;
    return float4(scene, 1.f);
}

// 2B dodge: fine vertical gold streaks and pale ground wisps. The original dash sequence
// invokes pl000f.eff/536; its texture bindings are not decoded, so these shapes are procedural.
float g_EvadeStrength = 1.f;

struct EV_VS_OUT
{
    float3 worldPosition : POSITION;
    float2 lifetime : TEXCOORD0;
    float size : TEXCOORD1;
    uint kind : TEXCOORD2;
};
struct EV_GS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float life : TEXCOORD1;
    nointerpolation uint kind : TEXCOORD2;
};

EV_VS_OUT VS_Evade(VS_IN input)
{
    EV_VS_OUT output;
    const float4x4 instanceWorld = float4x4(input.world0, input.world1, input.world2, input.world3);
    output.worldPosition = mul(mul(float4(input.position, 1.f), instanceWorld), g_WorldMatrix).xyz;
    output.lifetime = input.lifetime;
    output.size = length(input.world0.xyz);
    output.kind = (input.instanceId % 3 == 0) ? 0 : 1;
    // White filaments hug the floor; the gold streaks retain the random height.
    if (output.kind == 0) output.worldPosition.y = g_WorldMatrix._42 + 0.08f;
    return output;
}

[maxvertexcount(4)]
void GS_Evade(point EV_VS_OUT input[1], inout TriangleStream<EV_GS_OUT> stream)
{
    const uint kind = input[0].kind;
    const float3 center = input[0].worldPosition;
    float3 toCamera = g_CameraPosition.xyz - center;
    toCamera = normalize(toCamera + float3(0.0001f, 0.0001f, 0.0001f));
    const float3 right = normalize(cross(float3(0.f, 1.f, 0.f), toCamera));
    const float3 groundForward = normalize(float3(toCamera.x, 0.f, toCamera.z) + float3(0.0001f, 0.f, 0.0001f));
    const float3 up = kind == 1 ? float3(0.f, 1.f, 0.f) : groundForward;
    const float width = kind == 1 ? (0.045f + input[0].size * 0.07f) : (0.42f + input[0].size * 0.7f);
    const float height = kind == 1 ? (0.5f + input[0].size * 2.2f) : (0.10f + input[0].size * 0.18f);
    const float2 corners[4] = { float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(-0.5f, -0.5f), float2(0.5f, -0.5f) };
    const float2 uvs[4] = { float2(0.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(1.f, 1.f) };
    const float life = 1.f - saturate(input[0].lifetime.y / max(input[0].lifetime.x, 0.0001f));
    [unroll] for (uint i = 0; i < 4; ++i)
    {
        EV_GS_OUT output;
        output.position = mul(float4(center + right * corners[i].x * width + up * corners[i].y * height, 1.f),
                              mul(g_ViewMatrix, g_ProjMatrix));
        output.uv = uvs[i];
        output.life = life;
        output.kind = kind;
        stream.Append(output);
    }
}

float4 PS_Evade(EV_GS_OUT input) : SV_TARGET0
{
    float coverage;
    float3 color;
    if (input.kind == 1)
    {
        const float x = abs(input.uv.x - 0.5f);
        const float core = exp(-x * x * 950.f);
        const float halo = exp(-x * x * 90.f);
        const float ends = smoothstep(0.f, 0.18f, input.uv.y) * (1.f - smoothstep(0.76f, 1.f, input.uv.y));
        coverage = (core + halo * 0.28f) * ends;
        color = float3(1.f, 0.75f, 0.27f);
    }
    else
    {
        const float bend = 0.5f + sin(input.uv.x * 5.8f) * 0.16f;
        const float d = abs(input.uv.y - bend);
        coverage = exp(-d * d * 190.f) * sin(input.uv.x * 3.14159f);
        color = float3(0.92f, 0.96f, 0.82f);
    }
    const float strength = coverage * input.life * g_EvadeStrength;
    clip(strength - 0.008f);
    return float4(color * strength, strength);
}

// Firework: one burst per draw, one instance per star. Each star's way out is a Fibonacci-sphere direction turned
// by the burst seed; it flies out fast and slows under drag (most of the radius in the first half second), sags
// under gravity and dies over the life, so no per-star state is kept on the CPU. Style 0 is a chrysanthemum (thin
// trails of the last FIREWORK_TRAIL seconds of flight that twinkle as they die), style 1 a cluster of balls in
// mixed colours (the park's purple, green and white balls). g_QuadTint is the burst colour (a: brightness).
float4 g_FireworkBurst = float4(0.f, 0.f, 0.f, 0.f);    // xyz centre (world), w age (s)
float4 g_FireworkShape = float4(20.f, 2.f, 0.f, 0.f);   // x radius (m), y life (s), z seed, w style
uint g_FireworkStars = 1;

static const float FIREWORK_DRAG = 3.5f;
static const float FIREWORK_GRAVITY = 2.5f;
static const float FIREWORK_TRAIL = 0.35f;
static const float3 FIREWORK_BALL_COLORS[4] = { float3(0.75f, 0.35f, 1.f), float3(0.35f, 1.f, 0.45f),
    float3(1.f, 1.f, 1.f), float3(1.f, 0.4f, 0.75f) };

float3 Firework_Position(float3 direction, float speed, float age)
{
    return g_FireworkBurst.xyz + direction * speed * (1.f - exp(-FIREWORK_DRAG * age)) / FIREWORK_DRAG
        - float3(0.f, 0.5f * FIREWORK_GRAVITY * age * age, 0.f);
}

struct FW_VS_OUT
{
    float3 worldPosition : POSITION;
    float3 tailPosition : TEXCOORD0;
    float3 color : TEXCOORD1;
    float strength : TEXCOORD2;
    float size : TEXCOORD3;
};
struct FW_GS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;      // x across (-1..1), y along in half widths: 0 at the head, -trail at the tail
    float3 color : TEXCOORD1;
    float strength : TEXCOORD2;
    float trail : TEXCOORD3;
};

FW_VS_OUT VS_Firework(VS_IN input)
{
    const float i = (float)input.instanceId;
    const float seed = g_FireworkShape.z;
    const float k = i + 0.5f;
    const float y = 1.f - 2.f * k / max((float)g_FireworkStars, 1.f);
    const float ring = sqrt(saturate(1.f - y * y));
    const float phi = k * 2.399963f + seed * 6.2831f;
    const float3 direction = float3(ring * cos(phi), y, ring * sin(phi));

    const bool balls = g_FireworkShape.w > 0.5f;
    const float age = g_FireworkBurst.w;
    const float life = g_FireworkShape.y * lerp(0.8f, 1.1f, Node_Hash(i + seed * 17.f));
    const float speed = g_FireworkShape.x * FIREWORK_DRAG * lerp(balls ? 0.5f : 0.85f, 1.f, Node_Hash(i * 3.1f + seed));

    FW_VS_OUT output;
    output.worldPosition = Firework_Position(direction, speed, age);
    output.tailPosition = balls ? output.worldPosition : Firework_Position(direction, speed, max(age - FIREWORK_TRAIL, 0.f));
    const float fade = saturate(1.f - age / life);
    const float twinkle = balls ? 1.f : lerp(1.f, 0.35f + 0.65f * step(0.5f, frac(age * 9.f + Node_Hash(i * 7.3f))), saturate(age / life * 2.f - 0.8f));
    output.strength = input.instanceId < g_FireworkStars ? fade * fade * twinkle : 0.f;
    output.color = balls ? FIREWORK_BALL_COLORS[(uint)(Node_Hash(i * 5.7f + seed) * 4.f) % 4] : g_QuadTint.rgb;
    output.size = g_FireworkShape.x * (balls ? 0.14f : 0.06f);
    return output;
}

[maxvertexcount(4)]
void GS_Firework(point FW_VS_OUT input[1], inout TriangleStream<FW_GS_OUT> stream)
{
    if (input[0].strength <= 0.003f)
        return;
    const float3 head = input[0].worldPosition;
    const float3 look = normalize(g_CameraPosition.xyz - head);
    // A streak runs from its tail to its head as seen from the camera; balls (and a streak seen end on) stay round.
    float3 along = head - input[0].tailPosition;
    along -= look * dot(along, look);
    const float trailLength = sqrt(dot(along, along));
    along = trailLength > 0.01f ? along / trailLength : normalize(float3(g_ViewMatrix._12, g_ViewMatrix._22, g_ViewMatrix._32));
    const float3 across = normalize(cross(along, look));
    const float halfWidth = input[0].size * 0.5f;
    const float trail = trailLength / halfWidth;
    // Corners in half widths: across -1..1, along from one width past the tail to one past the head.
    const float2 corners[4] = { float2(-1.f, 1.f), float2(1.f, 1.f), float2(-1.f, -1.f - trail), float2(1.f, -1.f - trail) };
    [unroll] for (uint c = 0; c < 4; ++c)
    {
        FW_GS_OUT output;
        output.position = mul(float4(head + (across * corners[c].x + along * corners[c].y) * halfWidth, 1.f),
                              mul(g_ViewMatrix, g_ProjMatrix));
        output.uv = corners[c];
        output.color = input[0].color;
        output.strength = input[0].strength;
        output.trail = trail;
        stream.Append(output);
    }
}

float4 PS_Firework(FW_GS_OUT input) : SV_TARGET0
{
    // A tight white-hot core inside a coloured line, both rounded at the ends; a streak dims toward its tail.
    const float along = input.uv.y > 0.f ? input.uv.y : min(input.uv.y + input.trail, 0.f);
    const float d2 = input.uv.x * input.uv.x + along * along;
    const float toHead = input.trail > 0.05f ? saturate(1.f + input.uv.y / input.trail) : 1.f;
    const float glow = exp(-d2 * 6.f) * (0.1f + 0.9f * toHead * toHead);
    const float core = exp(-d2 * 28.f) * toHead * toHead * toHead;
    // The colour carries the brightness (a); the core stays a thin white line so a fresh burst keeps its hue.
    const float3 color = (input.color * glow * g_QuadTint.a + core * 0.7f) * input.strength;
    clip(max(color.r, max(color.g, color.b)) - 0.002f);
    return float4(color, 0.f);
}

// Spark spray (SparkEffect): a hit's fine sparks in one draw, one instance each, placed from the instance index as
// Firework places its stars. Each flies out within a cone round the axis (spread 0: along it, 1: anywhere over its
// half), slows under drag and falls, and is drawn by GS_Firework as a streak back to where it was trail seconds before,
// so a fast spark is long and a slowing one shrinks toward a point. It leaves white-gold, cools to g_QuadTint (a:
// brightness) and flickers as it dies.
float4 g_SprayBurst = float4(0.f, 0.f, 0.f, 0.f);       // xyz origin (world), w age (s)
float4 g_SprayAim = float4(0.f, 1.f, 0.f, 0.5f);        // xyz axis (unit), w spread
float4 g_SprayMotion = float4(5.f, 10.f, 9.f, 3.f);     // xy speed range (m/s), z gravity (m/s^2), w drag (1/s)
float4 g_SprayLook = float4(0.5f, 0.05f, 0.03f, 0.f);   // x life (s), y width (m), z trail (s), w seed
uint g_SprayCount = 0;

static const float3 SPRAY_HOT = float3(1.f, 0.85f, 0.55f);

float3 Spray_Position(float3 velocity, float drag, float age)
{
    return g_SprayBurst.xyz + velocity * (1.f - exp(-drag * age)) / drag - float3(0.f, 0.5f * g_SprayMotion.z * age * age, 0.f);
}

FW_VS_OUT VS_Spray(VS_IN input)
{
    const float i = (float)input.instanceId;
    const float seed = g_SprayLook.w;
    // A random way out, folded onto the axis's half and drawn toward the axis as the spread narrows.
    const float z = Node_Hash(i * 1.37f + seed) * 2.f - 1.f;
    const float phi = Node_Hash(i * 2.71f + seed * 3.f) * 6.2831853f;
    const float ring = sqrt(saturate(1.f - z * z));
    float3 way = float3(ring * cos(phi), z, ring * sin(phi));
    const float3 axis = g_SprayAim.xyz;
    const float along = dot(way, axis);
    if (along < 0.f)
        way -= 2.f * along * axis;
    const float3 direction = normalize(lerp(axis, way, g_SprayAim.w));

    const float speed = lerp(g_SprayMotion.x, g_SprayMotion.y, Node_Hash(i * 3.91f + seed * 5.f));
    const float life = g_SprayLook.x * lerp(0.45f, 1.f, Node_Hash(i * 5.17f + seed * 7.f));
    const float drag = max(g_SprayMotion.w, 0.001f);
    const float age = g_SprayBurst.w;
    const float progress = saturate(age / life);

    FW_VS_OUT output;
    output.worldPosition = Spray_Position(direction * speed, drag, age);
    output.tailPosition = Spray_Position(direction * speed, drag, max(age - g_SprayLook.z, 0.f));
    const float fade = 1.f - progress;
    const float flicker = lerp(1.f, 0.35f + 0.65f * step(0.4f, frac(age * 14.f + Node_Hash(i * 7.3f + seed))), saturate(progress * 2.f - 0.6f));
    output.strength = input.instanceId < g_SprayCount ? fade * fade * flicker : 0.f;
    output.color = lerp(SPRAY_HOT, g_QuadTint.rgb, saturate(progress * 1.5f));
    output.size = g_SprayLook.y * lerp(0.6f, 1.2f, Node_Hash(i * 9.4f + seed * 11.f));
    return output;
}

// Confetti (ConfettiEffect): a park machine's fistful of paper, one instance per piece placed from its index. Each
// leaves within a cone round the throw, the air takes its speed and it settles to a slow swaying fall, comes to rest on
// the floor and fades at the end of its life. g_DiffuseTexture is the 4 x 30 atlas: a piece keeps one paper (column) and
// tumbles through the frames (rows) at its own rate.
float4 g_ConfettiBurst = float4(0.f, 0.f, 0.f, 0.f);   // xyz origin (world), w age (s)
float4 g_ConfettiAim = float4(0.f, 1.f, 0.f, 0.f);     // xyz throw (unit), w seed
float g_ConfettiFloor = 0.f;                           // world y the pieces rest on
uint g_ConfettiCount = 0;

static const float CONFETTI_LIFE = 4.f;     // ConfettiEffect::Life
static const float CONFETTI_DRAG = 2.2f;
static const float CONFETTI_FALL = 0.8f;    // settled fall speed (m/s)
static const float CONFETTI_SIZE = 0.17f;   // m

struct CF_VS_OUT
{
    float3 worldPosition : POSITION;
    float4 look : TEXCOORD0;     // x paper (column), y frame (row), z alpha, w roll
};
struct CF_GS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float alpha : TEXCOORD1;
};

CF_VS_OUT VS_Confetti(VS_IN input)
{
    const float i = (float)input.instanceId;
    const float seed = g_ConfettiAim.w;
    const float3 axis = g_ConfettiAim.xyz;
    const float3 side = normalize(abs(axis.y) < 0.99f ? cross(axis, float3(0.f, 1.f, 0.f)) : float3(1.f, 0.f, 0.f));
    const float3 side2 = cross(axis, side);
    // Up to about 60 degrees off the throw.
    const float cosine = lerp(0.5f, 1.f, Node_Hash(i * 1.37f + seed));
    const float phi = Node_Hash(i * 2.71f + seed * 3.f) * 6.2831853f;
    const float ring = sqrt(saturate(1.f - cosine * cosine));
    const float3 way = axis * cosine + (side * cos(phi) + side2 * sin(phi)) * ring;
    const float speed = lerp(2.f, 5.f, Node_Hash(i * 3.91f + seed * 5.f));

    // Thrown out and slowed by the air (v (1 - e^-dt) / d) while sinking toward the settled fall.
    const float age = g_ConfettiBurst.w;
    const float slow = (1.f - exp(-CONFETTI_DRAG * age)) / CONFETTI_DRAG;
    float3 position = g_ConfettiBurst.xyz + way * speed * slow;
    position.y -= CONFETTI_FALL * (age - slow);
    const float sway = sin(age * lerp(2.f, 4.f, Node_Hash(i * 8.1f + seed)) + Node_Hash(i * 7.3f + seed) * 6.2831853f);
    const bool landed = position.y <= g_ConfettiFloor + 0.02f;
    position += landed ? float3(0.f, 0.f, 0.f) : side * sway * 0.3f * saturate(age);
    position.y = max(position.y, g_ConfettiFloor + 0.02f);

    const float life = CONFETTI_LIFE * lerp(0.7f, 1.f, Node_Hash(i * 5.17f + seed * 7.f));
    const float tumble = lerp(15.f, 30.f, Node_Hash(i * 6.6f + seed));     // frames per second
    CF_VS_OUT output;
    output.worldPosition = position;
    output.look.x = floor(Node_Hash(i * 9.4f + seed * 11.f) * 4.f);
    // A piece on the floor lies still on its last frame.
    output.look.y = floor(fmod(Node_Hash(i * 4.4f + seed * 2.f) * 30.f + (landed ? 0.f : age * tumble), 30.f));
    output.look.z = input.instanceId < g_ConfettiCount ? saturate((life - age) / 0.6f) : 0.f;
    output.look.w = Node_Hash(i * 3.3f + seed) * 6.2831853f;
    return output;
}

[maxvertexcount(4)]
void GS_Confetti(point CF_VS_OUT input[1], inout TriangleStream<CF_GS_OUT> stream)
{
    if (input[0].look.z <= 0.003f)
        return;
    const float3 center = input[0].worldPosition;
    const float3 look = normalize(g_CameraPosition.xyz - center);
    float3 up = normalize(float3(g_ViewMatrix._12, g_ViewMatrix._22, g_ViewMatrix._32));
    float3 right = normalize(cross(up, look));
    up = normalize(cross(look, right));
    const float c = cos(input[0].look.w), s = sin(input[0].look.w);
    const float3 rolledRight = right * c + up * s;
    const float3 rolledUp = up * c - right * s;
    const float2 corners[4] = { float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(-0.5f, -0.5f), float2(0.5f, -0.5f) };
    const float2 uvs[4] = { float2(0.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(1.f, 1.f) };
    [unroll] for (uint k = 0; k < 4; ++k)
    {
        CF_GS_OUT output;
        const float3 worldPosition = center + (rolledRight * corners[k].x + rolledUp * corners[k].y) * CONFETTI_SIZE;
        output.position = mul(float4(worldPosition, 1.f), mul(g_ViewMatrix, g_ProjMatrix));
        output.uv = float2((input[0].look.x + uvs[k].x) / 4.f, (input[0].look.y + uvs[k].y) / 30.f);
        output.alpha = input[0].look.z;
        stream.Append(output);
    }
}

float4 PS_Confetti(CF_GS_OUT input) : SV_TARGET0
{
    float4 color = g_DiffuseTexture.Sample(LinearClampSampler, input.uv);
    color.a *= input.alpha;
    if (color.a < 0.05f) discard;
    return color;
}

technique11 DefaultTechnique
{
    pass Default
    {
        SetRasterizerState(RS_Default); SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = compile gs_5_0 GS_Main();
        PixelShader = compile ps_5_0 PS_Main();
    }
    pass Explode
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = compile gs_5_0 GS_Main();
        PixelShader = compile ps_5_0 PS_Explode();
    }
    pass Quad
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = compile gs_5_0 GS_Quad();
        PixelShader = compile ps_5_0 PS_Quad();
    }
    pass ShockRing
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        GeometryShader = compile gs_5_0 GS_Quad();
        PixelShader = compile ps_5_0 PS_ShockRing();
    }
    pass Evade
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Evade();
        GeometryShader = compile gs_5_0 GS_Evade();
        PixelShader = compile ps_5_0 PS_Evade();
    }
    pass Firework
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Firework();
        GeometryShader = compile gs_5_0 GS_Firework();
        PixelShader = compile ps_5_0 PS_Firework();
    }
    pass Spray
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Spray();
        GeometryShader = compile gs_5_0 GS_Firework();
        PixelShader = compile ps_5_0 PS_Firework();
    }
    pass Confetti
    {
        SetRasterizerState(RS_CullNone); SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Confetti();
        GeometryShader = compile gs_5_0 GS_Confetti();
        PixelShader = compile ps_5_0 PS_Confetti();
    }
}
