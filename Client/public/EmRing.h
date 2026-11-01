#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

// Rings of light whose band hurts and whose middle is safe (em300f 054/058/059 ring bomb, 085 jump stomp).
// BURST (Em3000 0560): a volley of `count` rings flashes out of the boss's head, one every `interval` seconds. Each is a
// thin deep-red laser line like EmLaser's beams; it opens out from startRadius while it floats up over the head and
// then drops onto the floor, crackling with lightning in a heat haze, and where it lands it bursts into a wall of fire.
// The rings land one after another, the first at firstRadius and the last at lastRadius.
// GROUND (Em3000 1500 landing, Em8000 0560/0561 stomps): one ring spreads from `start` over the floor at a steady speed.
// AREA (Em8000 0500 landing): a shockwave fills the floor out to endRadius at once (0500 seq: boxes of half extent 13
// turned 0/22/45/67 degrees, about a circle) and hurts anywhere inside it for AreaHitTime; its rim races out at
// spreadSpeed and the lit disc fades.
// GROUND and AREA can be drawn with a wider band and glow and a heat haze round the rim (Em8000's), their light into
// RENDERGROUP::EMISSIVE (glow). AREA can instead burst: no lit floor, blue gas flame bursting up out of its centre under
// a flare, only the haze running out (Em8000's).
// ROAR (Em3000's entrance roar, ev0301): no light and no hit, only an O-shaped ripple facing the camera that opens out from
// `start` (in the air: the head) to endRadius at spreadSpeed and dies, bending the screen across its band like a lens and
// pulling its red and blue apart (the "RoarRing" pass, haze strong).
// Only a grounded player on a band (inside the disc for AREA) is hit; jumping clears it. One object draws the whole volley in
// RENDERGROUP::DISTORTION: every ring's haze first, then their light, so no ring's haze (which repaints the screen copy)
// covers another ring's light where they cross on screen. With glow the light goes to EMISSIVE instead (added to the
// screen with the bloom over it) and only the haze stays in DISTORTION.
class CLIENT_DLL EmRing final : public GameObject
{
public:
	enum class MODE : uint8 { BURST, GROUND, AREA, ROAR };

	typedef struct tagEmRingDesc : public GAMEOBJECT_DESC
	{
		MODE mode{ MODE::BURST };
		Weak<GameObject> target{};
		Vector3 start{};			// the centre on the floor
		uint32 count{ 1 };			// BURST: rings in the volley
		Float interval{};			// BURST: seconds between them
		Float startRadius{ 0.5f };	// BURST: every ring's radius as it is born
		Float firstRadius{ 5.f };	// BURST: where the first ring bursts; the others step evenly to lastRadius
		Float lastRadius{ 5.f };
		Float firstFall{ 1.2f };	// BURST: how long the first ring flies before it bursts; the others step to lastFall
		Float lastFall{ 1.2f };
		Float endRadius{ 5.f };		// GROUND, ROAR: radius where it fades out; AREA: the disc's radius
		Float spreadSpeed{ 9.f };	// GROUND, AREA, ROAR
		// GROUND, AREA: the band's width across the ring (GROUND hurts on it) and its soft glow's; 0 keeps BandWidth and
		// GlowWidth (Em3000's). ROAR: the ripple's width once fully open.
		Float bandWidth{};
		Float glowWidth{};
		// GROUND, AREA: a heat haze this strong (BURST's is 1) over the floor round the band and up off it; 0: none.
		// ROAR: how hard the ripple bends and splits the screen (1: a shift of about 4% of the screen height).
		Float haze{};
		// GROUND, AREA: the light drawn into RENDERGROUP::EMISSIVE, so it glows through the bloom.
		Bool glow{ false };
		// AREA: a burst instead of the lit floor (no disc, band or fire wall): puffs of blue gas flame (Effect_BlueFlame,
		// data010 effect 336) about this many metres across bursting up out of the centre; 0: the lit disc.
		Float flame{};
		// AREA: a flare (Effect_Flare, data010 effect 300 stars and halos) this many metres across flashing up at the
		// centre as the shockwave is born and gone in FlashTime; 0: none.
		Float flash{};
		// GROUND, AREA, BURST: the screen glitches this hard (Game::Start_ScreenGlitch with bands) as the ring is born
		// (GROUND, AREA) or as each ring bursts (BURST), and its colours stay split while the rings are out; 0: neither.
		Float glitch{};
		Vector3 tint{ 1.f, 0.4f, 0.3f };
		Float damage{ 25.f };
		// BURST: played as each ring lands and bursts, on the ring where it passes nearest the target (empty: silent).
		wstring burstSound{};
	} EM_RING_DESC;

public:
	explicit EmRing() = default;
	explicit EmRing(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit EmRing(const EmRing& rhs);
	~EmRing() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

private:
	struct RING
	{
		Float spreadTime{};			// BURST: flight time before it bursts
		Float endRadius{};
		Float age{ -1.f };			// below zero until it is born
		Float radius{};
		Float height{};				// BURST: its height over the floor
		Float burstAge{ -1.f };		// BURST: time since it burst, below zero while it flies
		Float wallHeight{};
		Float opacity{};
		Bool hasHit{ false };
		Bool done{ false };
		Bool Is_Shown() const { return age >= 0.f && !done; }
	};

	void Update_Ring(RING& ring, Float timeDelta);
	// The player is hit within bandHalfWidth of the ring's radius, or anywhere inside it when filled.
	void Hit_Player(RING& ring, Float bandHalfWidth, Bool filled = false);
	void Play_BurstSound(const RING& ring) const;
	// A draw of [first, end) with the Effect_Em3000 image, or `srv` when it is set. Light goes to EMISSIVE with glow.
	void Add_Draw(const Char* pass, uint32 image, uint32 first, uint32 end, Bool light = true,
		const ComPtr<ID3D11ShaderResourceView>& srv = nullptr);
	// The images of a sequence (TextureSettings tag) looked up by path, empty when the level has none.
	vector<ComPtr<ID3D11ShaderResourceView>> Find_Images(const wstring& tag) const;
	Bool Is_Burst() const { return m_Desc.mode == MODE::AREA && !m_FlameImages.empty(); }
	Float Get_BandWidth() const { return m_Desc.bandWidth > 0.f ? m_Desc.bandWidth : BandWidth; }
	Float Get_GlowWidth() const { return m_Desc.glowWidth > 0.f ? m_Desc.glowWidth : GlowWidth; }

private:
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	ComPtr<ID3D11Buffer> m_VertexBuffer{};
	EM_RING_DESC m_Desc{};
	vector<RING> m_Rings{};
	Vector3 m_Center{};
	Float m_Age{};
	struct DRAW { const Char* pass; uint32 image, first, count; Bool light; ComPtr<ID3D11ShaderResourceView> srv; };
	array<DRAW, 20> m_Draws{};
	vector<ComPtr<ID3D11ShaderResourceView>> m_FlameImages{};
	vector<ComPtr<ID3D11ShaderResourceView>> m_FlareImages{};
	Float m_FlashRoll{};
	struct PUFF { Vector3 offset; Float delay, rise, size, roll; };
	vector<PUFF> m_Puffs{};
	uint32 m_NumDraws{};

	static constexpr Float BurstStartHeight = 6.9f;		// BURST: the head (0560's attack box height)
	static constexpr Float RiseHeight = 4.f;			// BURST: how far a ring floats up before it falls
	static constexpr Float RisePortion = 0.3f;			// BURST: the share of its flight spent floating up
	static constexpr Float FadeInTime = 0.15f;
	static constexpr Float BurstTime = 0.7f;			// BURST: the fire wall's rise and fade
	static constexpr Float BurstHitWindow = 0.3f;
	static constexpr Float BurstWallHeight = 2.6f;
	static constexpr Float BurstVolume = 0.3f;			// BURST: burstSound, as Bullet's ARC bursts
	static constexpr Float BurstSoundNear = 10.f;
	static constexpr Float BurstSoundFar = 80.f;
	// With glitch: a brief pulse at birth or burst, fading independently of the ring lifetime.
	static constexpr Float GlitchTime = 0.3f;
	// BURST: where a ring bursts a ripple runs out over the floor BurstRippleSpread metres over BurstTime, bending the
	// screen and splitting its red and blue as ROAR's does ("RoarRing" pass), fading as it goes.
	static constexpr Float BurstRippleSpread = 3.f;
	static constexpr Float BurstRippleWidth = 1.6f;
	static constexpr Float BurstRippleStrength = 0.8f;
	static constexpr Float GroundWallHeight = 0.7f;
	static constexpr Float GroundFadeTime = 0.4f;
	static constexpr Float GroundStartRadius = 1.f;
	static constexpr Float AreaHitTime = 3.f / 60.f;	// AREA: 0500 seq f135-137
	static constexpr Float AreaFadeTime = 0.5f;		// AREA: the lit disc's fade once its rim is out
	static constexpr Float RoarFadeInTime = 0.05f;	// ROAR: the ripple's fade in, then out (square of what is left) as it opens
	static constexpr Float RoarStartWidth = 0.3f;	// ROAR: its width at birth, a share of bandWidth
	static constexpr Float DiscBrightness = 1.2f;		// AREA: the disc's light at its rim, a third of that at the middle
	// AREA's burst: PuffCount puffs of 336, each played once over FlameTime (its frames burst and thin out by themselves)
	// from up to PuffDelay late, up to PuffSpread flames out from the centre, rising PuffMinRise-PuffMaxRise m/s and
	// growing from half to PuffMinSize-PuffMaxSize flames across. FlameTint keeps 336's blue and makes it hot. Tuned on
	// screen against the reference (youtube 7rjrkycOCXo 2:15, 2:33).
	static constexpr Float FlameTime = 0.6f;
	static constexpr uint32 PuffCount = 9;
	static constexpr Float PuffDelay = 0.1f;
	static constexpr Float PuffSpread = 1.2f;
	static constexpr Float PuffMinRise = 4.f;
	static constexpr Float PuffMaxRise = 10.f;
	static constexpr Float PuffMinSize = 1.2f;
	static constexpr Float PuffMaxSize = 2.4f;
	static inline const Vector3 FlameTint{ 1.2f, 1.2f, 2.2f };
	// AREA's flash: Effect_Flare's layers (0 starburst, 1 star with rainbow arcs, 2 rainbow halo, 3 cross glint, 4 soft
	// glow), popping to full size in the first FlashPop of FlashTime and fading after; lifted FlashHeight over the floor
	// and pulled toward the camera (half its size, at most half the way) so the floor doesn't cut it.
	static constexpr Float FlashTime = 0.3f;
	static constexpr Float FlashPop = 0.12f;
	static constexpr Float FlashHeight = 2.f;
	static constexpr Float FlashBrightness = 2.5f;	// times the ring's tint (Em8000's: the stomp rings' orange)

	static constexpr Float BandWidth = 0.6f;			// the glowing band's width across the ring
	static constexpr Float GlowWidth = 2.2f;			// GROUND: its soft glow
	static constexpr Float GroundHazeSpread = 1.5f;		// GROUND, AREA: the haze over the floor, this many glows wide
	static constexpr Float GroundHazeHeight = 2.f;		// GROUND, AREA: and up off the band, fading upward
	static constexpr Float FloorLift = 0.05f;
	static constexpr Float PlayerRadius = 0.4f;
	static constexpr Float JumpClearance = 0.9f;

	// em300f 054/059: a solid-coloured band (249/0 on the flat torus ef0008) and a fire wall (389/1 on ef0040).
	static constexpr uint32 GlowImage = 1;			// 000/0, read through its middle column for a soft cross-section
	static constexpr uint32 FireImage = 5;			// 389/1
	static constexpr uint32 LightningFirst = 7;		// 036/0..5, swapped in turn as on the bullets
	static constexpr uint32 LightningImages = 6;
	static constexpr uint32 HazeImage = 13;			// TSN 004/2
	static constexpr uint32 RippleImage = 14;		// coreeff 007/1: the ripples' ring ("RoarRing" pass)
	static constexpr Float BandBrightness = 4.f;
	static constexpr Float GlowBrightness = 0.8f;
	static constexpr Float FireBrightness = 3.f;

	// BURST's falling rings, drawn like EmLaser's beams: a narrow deep-red glow blended over the scene, a brighter red
	// band and a hot core added over it. Each layer is drawn twice, upright and flat, so it reads as a line from any angle.
	static constexpr Float RingGlowWidth = 1.f;
	static constexpr Float RingBandWidth = 0.4f;
	static constexpr Float RingCoreWidth = 0.14f;
	static inline const Vector3 RingGlowTint{ 0.75f, 0.03f, 0.08f };	// blended: a colour, not a brightness
	static inline const Vector3 RingBandTint{ 2.4f, 0.2f, 0.3f };
	static inline const Vector3 RingCoreTint{ 6.f, 2.f, 2.2f };
	static constexpr Float HazeWidth = 2.4f;
	static constexpr Float HazeStrength = 1.f;
	static constexpr Float HazeTiling = 0.5f;			// noise repeats per metre along the ring
	static constexpr Float LightningRate = 12.f;		// image swaps per second
	static constexpr Float LightningSize = 1.f;			// one bolt image's square on the ring
	static inline const Vector3 LightningTint{ 1.8f, 0.35f, 0.45f };
	static constexpr Float BirthFlash = 2.f;			// extra lightning as a ring is born, dying away
	static constexpr uint32 MaxLightningTiles = 160;

	static constexpr uint32 Segments = 72;
	static constexpr uint32 VerticesPerLayer = Segments * 6;
	// Per BURST ring: three haze layers (upright, flat, over the wall), glow, band and core twice each, the ripple, and the
	// bolts upright and flat. GROUND needs less.
	static constexpr uint32 VerticesPerRing = VerticesPerLayer * 10 + MaxLightningTiles * 2 * 6;

public:
	static Shared<EmRing> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
