#pragma once
#include "ParticleEffect.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL SparkEffect final : public ParticleEffect
{
public:
	typedef struct tagSparkEffectDesc : public PARTICLE_EFFECT_DESC
	{
		ATK_TYPE atkType{};
		Vector3 position{};
		Quaternion rotation{ 0.f, 0.f, 0.f, 1.f };
		Bool refract{ false };		// a ricochet: a ring facing the camera also bends the screen around the hit
		// Non-zero: a heavy blow on the floor (a boss's foot, blade, tail or body coming down): metal thrown up and out
		// all round position, this many times a foot's spray (more sparks, flung farther, a bigger flash and ring).
		Float impact{ 0.f };
		// Non-zero: electricity crackling over a machine at position, a crackle (Effect_Electric, data010 effect 327)
		// this many metres across flickering through its frames, with a few small yellow sparks jumping off it.
		Float electric{ 0.f };
		// A blow on the floor and electricity glow (RENDERGROUP::EMISSIVE: added to the screen with the bloom over it)
		// and glint: their flash is Effect_Flare's stars (data010 effect 300) and their sparks twinkle at the head.
	} SPARK_EFFECT_DESC;

public:
	explicit SparkEffect();
	explicit SparkEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SparkEffect(const SparkEffect& rhs);
	~SparkEffect() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override { ParticleEffect::On_Destroy(); }
	void On_Enable() override { ParticleEffect::On_Enable(); }
	void On_Disable() override { ParticleEffect::On_Disable(); }

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	void Submit_RenderGroup() override;
	HRESULT Render() override;

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

private:
	// Each spark keeps one of the texture's variants (Metal_Spark frames are separate shapes, not an animation)
	// and stands along the way it flies.
	struct SPARK
	{
		uint32 frame{};
		Vector3 position{};
		Vector3 velocity{};
		Vector3 direction{};
		Float lifeTime{};
		Float height{};		// length at launch speed
		Float launchSpeed{};
	};
	static constexpr uint32 MaxSparks = 64;
	// A flying spark stretches with its speed against its launch speed: shorter as it slows at the top of its arc,
	// longer as it falls, StretchScale times that ratio within these bounds. Tuned on screen.
	static constexpr Float StretchScale = 1.6f;
	static constexpr Float StretchMin = 0.35f;
	static constexpr Float StretchMax = 2.5f;
	// Every streak starts burning away in bits at this share of its life (Shader_VtxParticlePoint g_QuadDissolve).
	static constexpr Float DissolveFrom = 0.3f;
	// The spray (Shader_VtxParticlePoint Spray pass): fine sparks thrown out of the hit all at once over the streaks, one
	// draw for all of them; the shader places each from its instance index (as Firework does its stars), so none is
	// kept here. Each flies out within a cone round the hit's axis (spread 0: along it, 1: anywhere over its half), slows
	// under drag, falls, and draws as a streak back to where it was SprayTrail seconds before. A blow on the floor throws
	// impact times the count up and out, sqrt(impact) times as fast. Electricity has none. Tuned on screen.
	struct SPRAY_SHAPE
	{
		uint32 count{};
		Float spread{}, minSpeed{}, maxSpeed{}, gravity{}, drag{}, life{}, width{};
	};
	static constexpr SPRAY_SHAPE SwordSpray{ 120, 0.55f, 8.f, 22.f, 9.f, 3.f, 0.4f, 0.05f };
	static constexpr SPRAY_SHAPE PodSpray{ 48, 0.6f, 6.f, 14.f, 9.f, 3.f, 0.3f, 0.04f };
	static constexpr SPRAY_SHAPE ImpactSpray{ 128, 0.9f, 6.f, 16.f, 18.f, 1.5f, 0.8f, 0.06f };
	static constexpr uint32 MaxSpray = 384;
	static constexpr Float SprayTrail = 0.035f;
	static inline const Vector4 SprayTint{ 1.f, 0.4f, 0.12f, 3.f };	// cooled colour (a: brightness); they leave white-gold
	// A foot's blow (impact 1): this many sparks flung at these speeds, up from these angles over the floor, pulled down
	// harder than a Pod hit's so they arc back to the floor within their life. Tuned on screen, not source data.
	static constexpr uint32 ImpactSparks = 32;
	static constexpr Float ImpactMinSpeed = 9.f;
	static constexpr Float ImpactMaxSpeed = 18.f;
	static constexpr Float ImpactMinAngle = 0.25f;
	static constexpr Float ImpactMaxAngle = 1.2f;
	static constexpr Float ImpactGravity = 18.f;
	static constexpr Float ImpactGlowTime = 0.15f;
	// From this impact up the blow also bends the screen in a ring (the ricochet's), as large as the impact.
	static constexpr Float ImpactRefractFrom = 2.f;
	// Such a blow also glitches the screen a little (Game::Start_ScreenGlitch). Tuned on screen.
	static constexpr Float ImpactGlitch = 0.35f;
	static constexpr Float ImpactGlitchTime = 0.25f;
	// Electricity: the crackle's frames run at ElectricRate from a random one for a random time in this range, turned
	// at random; ElectricSparks small sparks fly off it. Orange like em8000's stomp rings (on request). Tuned on screen.
	static constexpr Float ElectricRate = 30.f;
	static constexpr Float ElectricMinTime = 0.2f;
	static constexpr Float ElectricMaxTime = 0.4f;
	static constexpr uint32 ElectricSparks = 6;
	static inline const Vector4 ElectricTint{ 4.f, 1.8f, 0.8f, 2.5f };
	static inline const Vector4 ElectricSparkTint{ 5.f, 2.25f, 1.f, 4.f };
	// A blow on the floor's sparks, in the same orange.
	static inline const Vector4 ImpactSparkTint{ 5.f, 2.25f, 1.f, 4.f };
	// Glints (Effect_Flare): the blow's flash a cross star (3) FlareSize times the glow over a starburst (0) for
	// FlareTime; electricity a soft glow (4) under the crackle; every glowing spark a small cross star at its
	// head, GlintSize times its length, twinkling GlintRate times a second while it lives. Tuned on screen.
	enum FLARE_IMAGE : uint32 { FLARE_STARBURST = 0, FLARE_CROSS = 3, FLARE_GLOW = 4, FLARE_END = 6 };
	static constexpr Float FlareTime = 0.3f;
	static constexpr Float FlareSize = 1.5f;
	static constexpr Float GlintSize = 0.3f;
	static constexpr Float GlintRate = 9.f;
	static inline const Vector4 FlareTint{ 3.f, 1.35f, 0.6f, 1.f };
	static inline const Vector4 ElectricGlowTint{ 1.2f, 0.54f, 0.24f, 1.f };
	// The white-hot flash at a Pod hit that the sparks fly out of.
	static constexpr Float GlowTime = 0.1f;
	static constexpr Float GlowSize = 0.8f;
	// A ricochet's ring: Pl0000Shockwave's ShockRing pass stood up to face the camera. It is pulled toward the camera by
	// RefractLift so the surface it glanced off doesn't hide the ring's far half.
	static constexpr Float RefractTime = 0.45f;
	static constexpr Float RefractSize = 3.f;
	static constexpr Float RefractStrength = 4.f;
	static constexpr Float RefractEmissive = 1.f;
	static constexpr Float RefractLift = 1.5f;
	// Sword hits also leave the cut itself (coreeff 031): a white-hot line along the swing over an orange haze,
	// sparks strung along it and a burst at the hit point.
	enum SLASH_FRAME : uint32 { SLASH_BURST, SLASH_LINE, SLASH_HAZE, SLASH_SPARKS, SLASH_END };
	static constexpr Float SlashTime = 0.15f;
	// The line covers about 3/4 of its frame, so a 5 m quad shows a cut about 3.75 m long.
	static constexpr Float SlashLength = 5.f;
	static constexpr uint32 NumLineSparks = 3;

	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Particle_Point>  m_Buffer{ nullptr };
	SPARK m_Sparks[MaxSparks]{};
	uint32 m_NumSparks{};
	SPRAY_SHAPE m_Spray{};		// count 0: no spray
	Vector3 m_SprayAxis{};
	// Sword hits: long streaks rooted at the hit point; Pod hits: a shower of small streaks thrown out of it.
	Bool m_AnchorBottom{ false };
	Bool m_Refract{ false };
	Float m_Impact{ 0.f };
	Float m_Gravity{ 6.f };
	Float m_Electric{ 0.f };
	uint32 m_ElectricFrame{};
	Float m_ElectricRoll{};
	// The crackle's frames are looked up by path as they come up (the one Texture component holds the sparks').
	uint32 m_ElectricLevel{};
	wstring m_ElectricPath{};
	uint32 m_ElectricFrames{};
	// Effect_Flare, looked up by path like the crackle; empty when the level has none (no glints then).
	ComPtr<ID3D11ShaderResourceView> m_FlareSRVs[FLARE_END]{};
	Float m_FlareRoll{};
	Bool Is_Glowing() const { return m_Impact > 0.f || m_Electric > 0.f; }
	Float m_Duration{ 0.f };
	Vector3 m_HitPosition{};
	Float m_Age{ 0.f };
	// A GameObject holds one Texture component (the sparks'), so the slash frames are looked up the way Texture does.
	ComPtr<ID3D11ShaderResourceView> m_SlashSRVs[SLASH_END]{};
	Vector3 m_SlashAxis{};
	Float m_SlashRoll{ 0.f };
	Float m_LineSparkOffsets[NumLineSparks]{};

public:
	static Shared<SparkEffect> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
