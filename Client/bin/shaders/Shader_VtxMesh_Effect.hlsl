#include "EngineShaderDefine.hlsli"
#include "EngineShaderVertex.hlsli"

// Effect passes drawn from CPU-built quads in world space (world matrix = identity).
// Per-vertex data: normal = HDR tint, texcoord1.x = opacity, texcoord1.y = distance along a beam in metres.
// Textures are coreeff.dtt images copied byte for byte (Texture/Effect/PodLaser).
Texture2D g_EffectMap;      // sprite image, or the beam's core line (306/31)
Texture2D g_EffectGlowMap;  // beam outer band (104/30)
Texture2D g_EffectFlowMap;  // beam streak panned along its length (306/5)
float g_EffectTime;
Texture2D g_SceneTexture;   // lit screen copied before RENDERGROUP::DISTORTION
float4 g_EffectColor;       // model effects (VS_Mesh): rgb tint, a opacity; their normals are real normals
float4 g_EffectGlowColor;   // bullets: the enemy ball's rim
Texture2D g_BulletPatternMap;   // enemy ball inside: 009/1 fiery mottle, or 009/0 smoke curls (unbreakable)
Texture2D g_BulletLightningMap; // enemy ball: one of 036/0..5, branching lightning (alpha)
// Sword slash textures: the TSC/TSM/TSN settings of core.eff/595 and wpf000.eff/111 (the trail, on ef0901), which the
// glass wave reuses for its own patterns.
Texture2D g_SlashColorMap;  // TSC 006/0, smoke
Texture2D g_SlashMaskMap;   // TSM 008/1, fine streaks (alpha)
Texture2D g_SlashNormalMap; // TSN 004/2, refraction normals
// The slash crescents of coreeff 007 (alpha, each an arc about the image centre at 0.41 of its width): 3 and 4 torn
// smoky arcs, 6 a soft smear, 5 a sharp line.
Texture2D g_SlashArcMap0;   // 007/3
Texture2D g_SlashArcMap1;   // 007/4
Texture2D g_SlashArcMap2;   // 007/6
Texture2D g_SlashArcLineMap;// 007/5
float4 g_SlashArc;          // x: the image's turn (radians), y: which torn arc (0 or 1) or the haze's strength,
                            // z: wear, w: noise seed

typedef VTXMESH VS_IN;
struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 tint : TEXCOORD1;
    float opacity : TEXCOORD2;
    float along : TEXCOORD3;
};

VS_OUT VS_Main(VS_IN input)
{
    BEGIN_OBJECT_WORLD(input);
    VS_OUT output;
    output.position = Node_ObjectToClip(input.position);
    output.uv = input.texcoord;
    output.tint = input.normal;
    output.opacity = input.texcoord1.x;
    output.along = input.texcoord1.y;
    return output;
}

VS_OUT VS_Mesh(VS_IN input)
{
    BEGIN_OBJECT_WORLD(input);
    VS_OUT output;
    output.position = Node_ObjectToClip(input.position);
    output.uv = input.texcoord;
    output.tint = g_EffectColor.rgb;
    // The wmb vertex alpha that fades the crescent's edges, stored as an OBJ vertex colour (r = g = b = alpha).
    output.opacity = g_EffectColor.a * input.color.r;
    output.along = 0.f;
    return output;
}

// uv.x runs along the beam (muzzle 0 -> end 1), uv.y across it. Both line images are vertical, so
// their x axis gives the cross-section; the streak's y axis is panned along the beam (wp0630-style flow).
// The beam leaves the pod as a thin needle and reaches full width within 1.2 m: the cross-section is
// squeezed into the narrower width instead of being cut off.
float BeamAcross(VS_OUT input)
{
    float width = lerp(0.15f, 1.f, saturate(input.along / 1.2f));
    return 0.5f + (input.uv.y - 0.5f) / width;
}

float4 PS_LaserBeam(VS_OUT input) : SV_TARGET0
{
    float v = BeamAcross(input);
    clip(0.5f - abs(v - 0.5f));
    float core = g_EffectMap.Sample(LinearClampSampler, float2(v, 0.5f)).a;
    float band = g_EffectGlowMap.Sample(LinearClampSampler, float2(v, 0.5f)).a;
    float flow = g_EffectFlowMap.Sample(LinearWrapSampler,
        float2(v, input.along * 0.35f - g_EffectTime * 6.f)).a;
    float3 hot = input.tint * core * (0.85f + 0.3f * flow);
    float3 warm = float3(1.6f, 0.62f, 0.16f) * band * (0.45f + 0.55f * flow);
    return float4((hot + warm) * input.opacity, 0.f);
}

float4 PS_Additive(VS_OUT input) : SV_TARGET0
{
    float4 color = g_EffectMap.Sample(LinearClampSampler, input.uv);
    return float4(color.rgb * color.a * input.tint * input.opacity, 0.f);
}

// Additive with a tiling image (Em3000Ring's fire wall wraps its noise around the ring and scrolls it upward).
float4 PS_AdditiveWrap(VS_OUT input) : SV_TARGET0
{
    float4 color = g_EffectMap.Sample(LinearWrapSampler, input.uv);
    return float4(color.rgb * color.a * input.tint * input.opacity, 0.f);
}

// Heat sizzle in screen space for Em3000's laser and rings (g_EffectMap is the TSN 004/2 normal noise): two layers of
// the noise, about 40 and 15 pixels across, scrolling past each other fast. Roughly -1..1 in x and y.
float2 Sizzle(float2 pixel)
{
    float2 p = pixel / 40.f;
    float2 a = g_EffectMap.Sample(LinearWrapSampler, p + float2(0.f, g_EffectTime * 3.f)).xy * 2.f - 1.f;
    float2 b = g_EffectMap.Sample(LinearWrapSampler, p * 2.6f - float2(g_EffectTime * 2.3f, 0.f)).xy * 2.f - 1.f;
    return a + b * 0.6f;
}

// Em3000's radial laser, its guide lines and its rings, without an image: uv.x runs along a strip (0 at the boss), uv.y
// across it. The cross-section is a gaussian, so a narrow strip reads as the hot core and a wide one as its glow; the far
// end fades out. The sizzle shakes the cross-section sideways and flickers its brightness, so the edges seethe.
float SoftBeam_Shape(VS_OUT input, float wobble)
{
    float across = input.uv.y * 2.f - 1.f + wobble;
    float ends = smoothstep(0.f, 0.01f, input.uv.x) * (1.f - smoothstep(0.55f, 1.f, input.uv.x));
    return exp(-6.f * across * across) * ends * input.opacity;
}

float4 PS_SoftBeam(VS_OUT input) : SV_TARGET0
{
    float2 sizzle = Sizzle(input.position.xy);
    return float4(input.tint * SoftBeam_Shape(input, sizzle.x * 0.25f) * max(1.f + sizzle.y * 0.5f, 0.f), 0.f);
}

// The same shape blended over the scene in tint (a colour): the laser's deep-red glow, which added light would turn
// orange over the grass.
float4 PS_SoftBeamBlend(VS_OUT input) : SV_TARGET0
{
    float2 sizzle = Sizzle(input.position.xy);
    float alpha = saturate(SoftBeam_Shape(input, sizzle.x * 0.25f) * (1.f + sizzle.y * 0.3f));
    clip(alpha - 0.004f);
    return float4(input.tint, alpha);
}

// Em3000's laser and rings in RENDERGROUP::DISTORTION: heat haze around them. g_EffectMap is the TSN 004/2 normal noise,
// tiled along the strip (uv.x, half a metre a repeat) and across it (uv.y), in two layers scrolled fast past each other so
// the scene behind shimmers; the shift (up to about 3% of the screen height) and the blend fade with a gaussian across
// the strip. tint.r is its strength.
float4 PS_RingHaze(VS_OUT input) : SV_TARGET0
{
    float across = input.uv.y * 2.f - 1.f;
    float weight = exp(-6.f * across * across) * input.tint.r;
    clip(weight - 0.01f);
    float2 flow = float2(g_EffectTime * 1.3f, -g_EffectTime * 4.f);
    float2 broad = g_EffectMap.Sample(LinearWrapSampler, input.uv * float2(2.f, 1.2f) + flow).xy * 2.f - 1.f;
    float2 fine = g_EffectMap.Sample(LinearWrapSampler, input.uv * 5.f - flow * 1.7f).xy * 2.f - 1.f;
    uint width, height;
    g_SceneTexture.GetDimensions(width, height);
    float2 source = (input.position.xy + (broad + fine * 0.8f) * (0.03f * height) * weight) / float2(width, height);
    return float4(g_SceneTexture.Sample(LinearClampSampler, source).rgb, saturate(weight * 4.f));
}

// EmRing ROAR (Em3000's entrance roar) and BURST's floor ripples: an O-shaped ripple. uv.y runs across its band (0
// inside, 1 outside), uv.x once round it, and tint.r is its strength. The band's shape is g_EffectMap's ring (coreeff
// 007/1, a thin crest in a soft glow) read along its radius from RoarRingInner out to its edge, the crest at
// RoarRingCrest across the band. As on Pl0000Shockwave's ShockRing the screen bends across the crest, pulled in inside
// it and pushed out outside, and its red and blue are pulled apart outward and inward. The outward direction on the
// screen is where uv.y grows.
static const float RoarRingInner = 0.62f;
static const float RoarRingCrest = 0.47f;
float4 PS_RoarRing(VS_OUT input) : SV_TARGET0
{
    float x = (input.uv.y - RoarRingCrest) * 2.f;
    float angle = input.uv.x * 6.2831853f;
    float radius = lerp(RoarRingInner, 1.f, input.uv.y) * 0.5f;
    float band = g_EffectMap.Sample(LinearClampSampler, 0.5f + float2(cos(angle), sin(angle)) * radius).a;
    float strength = band * input.tint.r;
    clip(strength - 0.005f);
    float2 grow = float2(ddx(input.uv.y), ddy(input.uv.y));
    float2 outward = grow / max(length(grow), 1e-6f);
    uint width, height;
    g_SceneTexture.GetDimensions(width, height);
    float2 size = float2(width, height);
    float2 shift = outward * (-x * strength) * (0.04f * height);
    float2 split = outward * strength * (0.012f * height);
    float r = g_SceneTexture.Sample(LinearClampSampler, (input.position.xy + shift + split) / size).r;
    float g = g_SceneTexture.Sample(LinearClampSampler, (input.position.xy + shift) / size).g;
    float b = g_SceneTexture.Sample(LinearClampSampler, (input.position.xy + shift - split) / size).b;
    return float4(r, g, b, saturate(band * 3.f) * saturate(input.tint.r * 4.f));
}

// Lightning crackling along Em3000's rings, added over them: one of 036/0..5 (alpha only), as on the bullets.
float4 PS_Lightning(VS_OUT input) : SV_TARGET0
{
    float bolt = g_EffectMap.Sample(LinearClampSampler, input.uv).a;
    return float4(input.tint * bolt * input.opacity, 0.f);
}

float4 PS_AlphaBlend(VS_OUT input) : SV_TARGET0
{
    float4 color = g_EffectMap.Sample(LinearClampSampler, input.uv);
    float alpha = color.a * input.opacity;
    clip(alpha - 0.004f);
    return float4(color.rgb * input.tint, alpha);
}

// The source streak has bright pixels at its rectangular top edge. Feather each thin strip
// on all four sides so the original pattern reads as hanging light rather than a card.
float4 PS_EvadeStreak(VS_OUT input) : SV_TARGET0
{
    float4 color = g_EffectMap.Sample(LinearClampSampler, input.uv);
    float side = saturate((0.5f - abs(input.uv.x - 0.5f)) / 0.22f);
    side = side * side * (3.f - 2.f * side);
    float ends = smoothstep(0.f, 0.08f, input.uv.y) * (1.f - smoothstep(0.7f, 1.f, input.uv.y));
    float shape = color.a * side * ends;
    clip(shape * input.opacity - 0.003f);
    float3 gold = float3(2.4f, 1.1f, 0.38f);
    float3 white = float3(2.25f, 2.15f, 1.9f);
    float3 glow = lerp(gold, white, side * side);
    float3 source = lerp(float3(0.72f, 0.67f, 0.55f), color.rgb, 0.35f);
    return float4(source * glow * shape * input.opacity, 0.f);
}

// The swing's trail (wpf000.eff/111 on ef0901), refraction only (TSN 004/2): the lit screen behind the crescent, pushed
// along the normal map's xy. ef0901 is a thin slab, a top and a bottom sheet wound opposite ways, so back faces are culled.
// Strength = TEX distortion (from the caller's tint.r) x vertex alpha x opacity. tint.g dims the scene where the noise
// bends hardest (the dark, ink-like smear).
float4 PS_SlashDistortion(VS_OUT input) : SV_TARGET0
{
    float strength = input.tint.r * input.opacity;
    clip(strength - 0.01f);
    // TSN 004/2 at the crescent's own UVs: the mesh covers a small, smoothly varying patch of it, so the whole arc
    // shifts the scene coherently (|xy| about 0.25 there). One UV is about 26 m on the scaled crescent, so the same
    // noise normals tiled x40 and x95 (blotches of roughly 0.2 m and 0.1 m), drifting apart, smear that shift into
    // blotches; the white layer's streak mask (TSM 008/1, lines along the arc) makes them run along the arc.
    float2 base = g_SlashNormalMap.Sample(LinearWrapSampler, input.uv).xy * 2.f - 1.f;
    float2 flow = float2(0.f, g_EffectTime * 1.5f);
    float2 blotch = g_SlashNormalMap.Sample(LinearWrapSampler, input.uv * 40.f + flow).xy * 2.f - 1.f;
    float2 grain = g_SlashNormalMap.Sample(LinearWrapSampler, input.uv * 95.f - flow * 1.7f).xy * 2.f - 1.f;
    float streak = g_SlashMaskMap.Sample(LinearClampSampler, input.uv).a;
    float2 noise = (blotch * 2.5f + grain * 1.5f) * (0.4f + 0.6f * streak);
    float2 normal = base + noise;
    // The shift is a fraction of the screen height so it reads the same at any resolution.
    uint width, height;
    g_SceneTexture.GetDimensions(width, height);
    float2 source = (input.position.xy + normal * (0.1f * height) * strength) / float2(width, height);
    float3 scene = g_SceneTexture.Sample(LinearClampSampler, source).rgb;
    float smudge = saturate(length(noise) * strength) * input.tint.g;
    return float4(scene * (1.f - smudge), 1.f);
}

// The flying sword wave (SwordWave.model mesh 1): a thick glass U made to match the in-game captures, since
// core.eff/595 on ef0901 only reads as thin brush strokes. uv.x runs along the U (thin tail 0 -> wide lead 1),
// uv.y across it (inner edge 0 -> outer edge 1); the vertex colour fades the tips. The section is a lens, so the
// normals lie down toward both edges, where the scene is bent hardest, as through thick glass.
struct GLASS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : TEXCOORD1;  // view space
    float opacity : TEXCOORD2;
};

// A little gold mixed into the glass's white: a touch in the crumples, more on the worn edges and in
// the glow.
static const float3 GlassGold = float3(1.f, 0.82f, 0.48f);

GLASS_OUT VS_Glass(VS_IN input)
{
    BEGIN_OBJECT_WORLD(input);
    GLASS_OUT output;
    output.position = Node_ObjectToClip(input.position);
    output.uv = input.texcoord;
    output.normal = mul(float4(Node_ObjectToWorldDirection(input.normal), 0.f), g_ViewMatrix).xyz;
    output.opacity = g_EffectColor.a * input.color.r;
    return output;
}

// The glow pass's shell: g_EffectColor.b swells the band within its own plane (object XZ, the in-plane part of the
// normals, which lie down toward both edges), so its light spreads past the edges while the top and bottom sheets
// stay together.
GLASS_OUT VS_GlassShell(VS_IN input)
{
    input.position += float3(input.normal.x, 0.f, input.normal.z) * g_EffectColor.b;
    return VS_Glass(input);
}

// 1 on the contour lines where frac(c) crosses one half, anti-aliased; width in contour units.
float Glass_Contour(float c, float width)
{
    float f = abs(frac(c) - 0.5f) * 2.f;
    return saturate(1.f - (f - width) / (fwidth(f) * 1.5f + 1e-4f));
}

// Clusters of crumples: a broad scale of the 004/2 noise normals, so the white gathers in patches with clear glass
// between them.
float Glass_Cluster(float s, float v)
{
    float2 broad = g_SlashNormalMap.Sample(LinearWrapSampler, float2(v * 0.5f + 0.61f, s * 2.5f)).xy;
    return saturate((dot(broad, float2(0.6f, 0.4f)) - 0.47f) * 5.f);
}

// The white of the glass, in the band's own space (s along it, v across it), like crumpled cellophane: sharp creases
// (contours of two scales of the 004/2 noise normals) and the flat crumpled faces between them, gathered in clusters
// and thicker toward the convex side, plus a broad bright rim along the convex edge broken up by the same noise.
// It drifts slowly. key orders how the glass wears away (Glass_Shade): clear glass first, the crumpled white last.
float Glass_White(float s, float v, out float key)
{
    float2 drift = float2(0.f, g_EffectTime * 0.3f);
    float2 big = g_SlashNormalMap.Sample(LinearWrapSampler, float2(v * 1.2f, s * 6.f) + drift).xy;
    float2 fine = g_SlashNormalMap.Sample(LinearWrapSampler, float2(v * 3.f + 0.37f, s * 15.f) - drift).xy;
    float density = (0.25f + 0.75f * v) * Glass_Cluster(s, v);
    float creases = max(Glass_Contour(big.x + 0.5f * fine.y, 0.06f), Glass_Contour(fine.x + 0.3f * big.y, 0.05f) * 0.7f);
    float faces = saturate((big.x * 0.55f + fine.y * 0.45f - 0.6f) * 25.f);
    float rim = saturate((v - 0.8f) / 0.08f) * saturate((1.f - v) / 0.03f) *
        (0.3f + 0.7f * saturate((big.y * 0.5f + fine.x * 0.5f - 0.45f) * 7.f));
    float white = saturate(max(rim, max(creases, faces) * density));
    key = saturate((big.x * 0.55f + fine.y * 0.45f - 0.3f) / 0.4f) * 0.65f + white * 0.25f + v * 0.1f;
    return white;
}

// Clear glass with crumpled white: rgb = its colour, a = how much of it is left at this pixel. The scene behind is bent
// as one piece by the lens normals (the band is thickest mid-band, so it bends hardest toward both edges), barely
// rippled by the noise, slightly greyed and lifted, with a faint sheen where the band is seen edge-on; the white
// (Glass_White) covers it toward white with a touch of gold, the same over any ground. As g_EffectColor.b (0..1) rises the
// glass wears away in the order of Glass_White's key, its worn edges turning white-gold, until none is left.
// g_EffectColor.r scales the white, g_EffectColor.g the bending.
float4 Glass_Shade(GLASS_OUT input)
{
    uint width, height;
    g_SceneTexture.GetDimensions(width, height);
    float s = input.uv.x;
    float v = input.uv.y;
    float3 n = normalize(input.normal);

    float2 bend = n.xy * float2(1.f, -1.f);
    bend *= min(1.f, 0.6f / (length(bend) + 1e-4f));
    float2 ripple = g_SlashNormalMap.Sample(LinearWrapSampler, float2(v * 1.5f, s * 5.f + g_EffectTime * 0.5f)).xy * 2.f - 1.f;
    float2 shift = (bend * 0.08f + ripple * 0.015f) * height * g_EffectColor.g * input.opacity;
    float3 scene = g_SceneTexture.Sample(LinearClampSampler, (input.position.xy + shift) / float2(width, height)).rgb;

    float grey = dot(scene, float3(0.299f, 0.587f, 0.114f));
    float sheen = pow(1.f - abs(n.z), 3.f) * 0.25f;
    float3 glass = lerp(scene, grey.xxx, 0.35f) * 0.9f + 0.05f + sheen;
    float key;
    glass = lerp(glass, lerp(1.f, GlassGold, 0.2f), saturate(Glass_White(s, v, key) * g_EffectColor.r));

    float worn = key + 0.05f - g_EffectColor.b * 1.05f;     // > 0 where glass is left
    glass = lerp(glass, lerp(1.f, GlassGold, 0.6f), saturate(1.f - worn / 0.1f) * saturate(g_EffectColor.b * 20.f) * 0.8f);
    float edge = saturate(v / 0.05f) * saturate((1.f - v) / 0.02f);
    return float4(glass, edge * saturate(worn / 0.04f));
}

// One pass in RENDERGROUP::DISTORTION: the flying wave over the scene copy.
float4 PS_GlassWave(GLASS_OUT input) : SV_TARGET0
{
    float4 glass = Glass_Shade(input);
    float3 behind = g_SceneTexture.Load(int3(input.position.xy, 0)).rgb;
    return float4(lerp(behind, glass.rgb, saturate(input.opacity) * glass.a), 1.f);
}

// Emissive light of the wave, added after GlassWave over a shell swollen by g_EffectColor.b: strongest along the
// convex rim and over the crumple clusters, squared so it fades out softly past the band's edges. g_EffectColor.r is
// its strength.
float4 PS_GlassGlow(GLASS_OUT input) : SV_TARGET0
{
    float s = input.uv.x;
    float v = saturate(input.uv.y);
    float glow = sqrt(sin(3.14159265f * v)) * (0.3f + 0.7f * smoothstep(0.4f, 0.9f, v)) * (0.6f + 0.4f * Glass_Cluster(s, v));
    return float4(lerp(1.f, GlassGold, 0.45f) * glow * glow * g_EffectColor.r * saturate(input.opacity), 0.f);
}

// The slash crescents of coreeff 007 on the wave mesh, projected from object XZ (the U's half circle about the origin,
// middle at +X, band 0.40..1.15 m) and turned so their arcs lie along it: image angle (counter-clockwise from right) =
// the U's angle (from +X toward +Z) + g_SlashArc.x.
static const float SlashArcSpan = 1.9f;    // metres one image width covers: its 0.41 arc on the band's middle (0.775 m)

struct ARC_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float opacity : TEXCOORD1;
};

ARC_OUT VS_SlashArc(VS_IN input)
{
    BEGIN_OBJECT_WORLD(input);
    ARC_OUT output;
    output.position = Node_ObjectToClip(input.position);
    float c = cos(g_SlashArc.x);
    float s = sin(g_SlashArc.x);
    float2 p = input.position.xz;
    float2 q = float2(c * p.x - s * p.y, s * p.x + c * p.y);
    output.uv = 0.5f + float2(q.x, -q.y) / SlashArcSpan;
    output.opacity = g_EffectColor.a * input.color.r;
    return output;
}

// A sheet torn off the wave (SwordWave): 007/3 or 007/4, torn by 004/2 noise read in polar coordinates (stretched
// along the arc, so it tears into tongues), more the higher the wear (g_SlashArc.z). As it wears its torn edge softens
// and it turns from a grey sheet bending the scene, with white torn edges and contour cracks, into faint warm smoke.
// 007/6 adds a faint grey smear. g_EffectColor: r = bend, g = ink, b = white, a = opacity.
float4 PS_SlashArc(ARC_OUT input) : SV_TARGET0
{
    float arc = g_SlashArc.y < 0.5f ? g_SlashArcMap0.Sample(LinearClampSampler, input.uv).a
        : g_SlashArcMap1.Sample(LinearClampSampler, input.uv).a;
    float haze = g_SlashArcMap2.Sample(LinearClampSampler, input.uv).a * 0.15f;
    // Angle with its seam pointing down the image, where neither torn arc reaches.
    float2 d = float2(input.uv.x - 0.5f, 0.5f - input.uv.y);
    float2 polar = float2(atan2(-d.x, d.y) / 6.2831853f, length(d));
    float seed = g_SlashArc.w;
    float2 noise = g_SlashNormalMap.Sample(LinearWrapSampler, polar * float2(2.f, 3.f) + float2(seed, seed * 0.7f)).xy;
    float key = saturate((dot(noise, float2(0.6f, 0.4f)) - 0.5f) * 2.2f + 0.5f);
    float tear = arc * (0.25f + 1.5f * key);
    float wear = g_SlashArc.z;
    float soft = saturate((wear - 0.3f) / 0.5f);
    float sheet = smoothstep(wear, wear + 0.12f + 0.35f * soft, tear);
    float edge = saturate((tear - wear) / 0.03f) * (1.f - saturate((tear - wear - 0.03f) / 0.08f)) * (1.f - soft);
    float2 fine = g_SlashNormalMap.Sample(LinearWrapSampler, polar * 6.f + float2(seed * 1.3f, 0.f)).xy;
    float crack = Glass_Contour(fine.x * 2.f + fine.y, 0.03f) * sheet * 0.7f;
    float alpha = saturate(max(max(sheet, edge), haze) * input.opacity);
    clip(alpha - 0.004f);

    uint width, height;
    g_SceneTexture.GetDimensions(width, height);
    float2 shift = (noise * 2.f - 1.f) * (0.03f * height) * g_EffectColor.r * sheet;
    float3 scene = g_SceneTexture.Sample(LinearClampSampler, (input.position.xy + shift) / float2(width, height)).rgb;
    float3 color = scene * (1.f - sheet * g_EffectColor.g * (1.f - soft) - haze) +
        float3(0.85f, 0.74f, 0.6f) * sheet * soft * 0.35f +
        float3(1.f, 1.f, 0.97f) * saturate(edge + crack) * g_EffectColor.b;
    return float4(color, alpha);
}

// Light added over the wave: white streaks along its convex side, 007/5's sharp line read at three radii (about 1.0,
// 1.05 and 1.11 m), in a soft haze of 007/6 that shimmers along the arc. g_EffectColor.r scales the streaks,
// g_SlashArc.y the haze.
float4 PS_SlashArcLine(ARC_OUT input) : SV_TARGET0
{
    float2 c = input.uv - 0.5f;
    float lines = g_SlashArcLineMap.Sample(LinearClampSampler, 0.5f + c * 0.74f).a +
        0.6f * g_SlashArcLineMap.Sample(LinearClampSampler, 0.5f + c * 0.78f).a +
        0.45f * g_SlashArcLineMap.Sample(LinearClampSampler, 0.5f + c * 0.7f).a;
    float2 polar = float2(atan2(-c.x, -c.y) / 6.2831853f, length(c));
    float shimmer = g_SlashNormalMap.Sample(LinearWrapSampler, polar * float2(3.f, 4.f) + float2(g_EffectTime * 0.6f, 0.f)).x;
    float haze = g_SlashArcMap2.Sample(LinearClampSampler, 0.5f + c * 0.8f).a * (0.5f + shimmer) * g_SlashArc.y;
    float3 light = float3(1.f, 1.f, 0.97f) * lines * g_EffectColor.r + lerp(float3(0.85f, 0.9f, 1.f), GlassGold, 0.25f) * haze;
    return float4(light * saturate(input.opacity), 0.f);
}

// Bullets: coreeff images laid over the candy sphere (radius about 1 about its origin) as a sprite facing the camera.
// The Pod's shot is only its core, stretched up to BulletStretch along its flight as seen from the side (round head-on),
// so the sphere swells to BulletGlowReach radii to hold the stretched core.
// The blended balls (Bullet, BulletSolid) write their sphere's depth: Bullet::Render draws each kind back to front, and
// the lightning and the other kind drawn after them stay behind a nearer ball.
static const float BulletStretch = 1.7f;
static const float BulletGlowReach = BulletStretch;
// The blue lightning around the enemy ball reaches a little past its rim.
static const float BulletLightningReach = 1.3f;

struct BULLET_OUT
{
    float4 position : SV_POSITION;
    float2 disc : TEXCOORD0;    // where the pixel lies over the sphere as the camera sees it, in sphere radii (x right, y up)
    float2 flight : TEXCOORD1;  // the flight direction (object +Z) on the screen, shorter the more it heads at the camera
    nointerpolation float time : TEXCOORD2;    // the bullet's age: g_EffectTime, or instance params.x when instanced
};

// Instanced bullets (Shader_VtxMeshInstance_Effect.hlsl, one draw per kind) carry their world matrix and age per instance.
#ifdef MESH_INSTANCED
#define BULLET_TIME(input) input.instanceParams.x
#else
#define BULLET_TIME(input) g_EffectTime
#endif

BULLET_OUT BulletSprite(VS_IN input, float reach)
{
    BEGIN_OBJECT_WORLD(input);
    BULLET_OUT output;
    output.time = BULLET_TIME(input);
    float4x4 worldView = mul(OBJECT_WORLD, g_ViewMatrix);
    output.position = Node_ObjectToClip(input.position * reach);
    float3 offset = mul(float4(input.position, 0.f), worldView).xyz;
    output.disc = offset.xy / length(offset) * reach;
    output.flight = normalize(mul(float4(0.f, 0.f, 1.f, 0.f), worldView).xyz).xy;
    return output;
}

BULLET_OUT VS_Bullet(VS_IN input)
{
    return BulletSprite(input, 1.f);
}

BULLET_OUT VS_BulletGlow(VS_IN input)
{
    return BulletSprite(input, BulletGlowReach);
}

BULLET_OUT VS_BulletLightning(VS_IN input)
{
    return BulletSprite(input, BulletLightningReach);
}

// The enemy ball's disc, 001/11: 0.34 inside, rising to 0.62 on its soft rim (0.9 radii out) and fading outward.
// The image is upright on the screen for every ball, as in the game.
float BulletDisc(BULLET_OUT input, out float2 uv)
{
    uv = 0.5f + float2(0.5f, -0.5f) * input.disc;
    return g_EffectMap.Sample(LinearClampSampler, uv).a;
}

// A ball that can be cut down, blended over the scene: salmon-orange (g_EffectColor, a its opacity) mottled with
// purple by 009/1 drifting over g_EffectTime, inside a thick pale rim (g_EffectGlowColor) from 0.8 radii out that
// fades outward.
float4 PS_Bullet(BULLET_OUT input) : SV_TARGET0
{
    float2 uv;
    float a = BulletDisc(input, uv);
    float r = length(input.disc);
    float outside = step(0.9f, r);
    float rim = max(max(saturate((a - 0.34f) / 0.28f), smoothstep(0.76f, 0.82f, r)), outside);
    float mottle = g_BulletPatternMap.Sample(LinearWrapSampler, uv * 1.2f + float2(0.07f, 0.11f) * input.time).a;
    float3 inside = lerp(float3(0.26f, 0.07f, 0.2f), g_EffectColor.rgb, smoothstep(0.05f, 0.45f, mottle));
    float alpha = lerp(lerp(g_EffectColor.a, g_EffectGlowColor.a, rim), a / 0.62f * g_EffectGlowColor.a, outside);
    clip(alpha - 0.004f);
    return float4(lerp(inside, g_EffectGlowColor.rgb, rim), alpha);
}

// A ball that can't be cut down, blended over the scene: dark (g_EffectColor) with pink smoke curls of 009/0 drifting
// inside, in a thick hot-pink rim (g_EffectGlowColor) from 0.7 radii out that fades past 0.92.
float4 PS_BulletSolid(BULLET_OUT input) : SV_TARGET0
{
    float2 uv;
    float a = BulletDisc(input, uv);
    float r = length(input.disc);
    float outside = step(0.92f, r);
    float rim = max(smoothstep(0.67f, 0.73f, r), outside);
    float curl = g_BulletPatternMap.Sample(LinearWrapSampler, uv * 0.9f + float2(0.05f, -0.04f) * input.time).a;
    float3 inside = lerp(g_EffectColor.rgb, g_EffectGlowColor.rgb * 0.85f, smoothstep(0.08f, 0.4f, curl) * 0.85f);
    float alpha = lerp(lerp(g_EffectColor.a, g_EffectGlowColor.a, rim), a / 0.62f * g_EffectGlowColor.a, outside);
    clip(alpha - 0.004f);
    return float4(lerp(inside, g_EffectGlowColor.rgb, rim), alpha);
}

// Blue lightning crackling over a ball that can be cut down, added over it: the 036 image the bullet picked (it swaps
// them to flicker), turning with g_EffectTime, in g_EffectColor (a its strength).
float4 PS_BulletLightning(BULLET_OUT input) : SV_TARGET0
{
    float2 c = input.disc / BulletLightningReach;
    float s, co;
    sincos(input.time * 2.5f, s, co);
    c = float2(c.x * co - c.y * s, c.x * s + c.y * co);
    float bolt = g_BulletLightningMap.Sample(LinearClampSampler, 0.5f + 0.5f * c).a;
    return float4(g_EffectColor.rgb * bolt * g_EffectColor.a, 0.f);
}

// The Pod's shot, added over the scene: only a white-hot core of 001/9 (soft-edged disc) in g_EffectColor, stretched
// along its flight. No halo: nothing around the core is coloured (#41).
float4 PS_BulletGlow(BULLET_OUT input) : SV_TARGET0
{
    float len = length(input.flight);
    float2 axis = len > 1e-4f ? input.flight / len : float2(0.f, 1.f);
    float2 p = float2(dot(input.disc, float2(axis.y, -axis.x)), dot(input.disc, axis) / lerp(1.f, BulletStretch, len));
    float core = g_EffectMap.Sample(LinearClampSampler, 0.5f + 0.5f * p).a;
    return float4(g_EffectColor.rgb * core * g_EffectColor.a, 0.f);
}

technique11 DefaultTechnique
{
    pass LaserBeam
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_LaserBeam();
    }
    pass Additive
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Additive();
    }
    pass AdditiveWrap
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_AdditiveWrap();
    }
    pass SoftBeam
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_SoftBeam();
    }
    pass SoftBeamBlend
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_SoftBeamBlend();
    }
    pass RingHaze
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_RingHaze();
    }
    pass RoarRing
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_RoarRing();
    }
    pass Lightning
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_Lightning();
    }
    pass AlphaBlend
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_AlphaBlend();
    }
    pass EvadeStreak
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Main();
        PixelShader = compile ps_5_0 PS_EvadeStreak();
    }
    pass SlashDistortion
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Mesh();
        PixelShader = compile ps_5_0 PS_SlashDistortion();
    }
    pass GlassWave
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Glass();
        PixelShader = compile ps_5_0 PS_GlassWave();
    }
    pass GlassGlow
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_GlassShell();
        PixelShader = compile ps_5_0 PS_GlassGlow();
    }
    pass SlashArc
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_SlashArc();
        PixelShader = compile ps_5_0 PS_SlashArc();
    }
    pass SlashArcLine
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_SlashArc();
        PixelShader = compile ps_5_0 PS_SlashArcLine();
    }
    pass Bullet
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Bullet();
        PixelShader = compile ps_5_0 PS_Bullet();
    }
    pass BulletSolid
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_Bullet();
        PixelShader = compile ps_5_0 PS_BulletSolid();
    }
    pass BulletLightning
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_BulletLightning();
        PixelShader = compile ps_5_0 PS_BulletLightning();
    }
    pass BulletGlow
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_TestNoWrite, 0);
        SetBlendState(BS_Additive, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);
        VertexShader = compile vs_5_0 VS_BulletGlow();
        PixelShader = compile ps_5_0 PS_BulletGlow();
    }
}
