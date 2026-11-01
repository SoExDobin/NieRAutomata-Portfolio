#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

// The bosses' lasers (em3000_laser_tele / laser_shot), a child of the boss reused by each of its laser attacks.
// Fire (Em3000 danmak 1): dark red guide lines first grow along the floor from under the skirt, then sixteen pink-white
// beams shoot out over them and sweep one slow full turn (reference video 1:08-1:19). A grounded player inside a beam is
// hit; jumping over it is safe.
// Fire_Aimed (Em8000 0701): one beam out of the eye, which the boss re-aims every frame (Set_Aim) while its body spins.
// The muzzle charges first, then the beam burns down to where it meets the floor and runs on over the ground (up and
// down its slopes, stopping at a wall), the caller's length in all and the caller's width; whatever it crosses is hit,
// and only a jump clears its run over the ground.
// The beams sizzle: the scene behind them shimmers in a heat haze and lightning crackles along them, as on EmRing;
// big sparks crackle along the guide lines too, and every muzzle crackles in a shimmer, charging up through the wind-up.
// Drawn in RENDERGROUP::DISTORTION for the haze.
class CLIENT_DLL EmLaser final : public GameObject
{
public:
	explicit EmLaser() = default;
	explicit EmLaser(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit EmLaser(const EmLaser& rhs);
	~EmLaser() override;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	// Starts the guide lines around the owner's feet; the beams and the sweep follow on their own.
	void Fire();
	// Starts the aimed beam from origin along direction down to floorY, then along the floor, length metres in all and
	// width wide. Held until Stop.
	void Fire_Aimed(const Vector3& origin, const Vector3& direction, Float floorY, Float width, Float length);
	void Set_Aim(const Vector3& origin, const Vector3& direction);
	void Stop();
	Bool Is_Running() const { return m_Age >= 0.f; }

private:
	Float Get_WarnTime() const { return m_Aimed ? AimedChargeTime : WarnTime; }
	Float Get_Sweep() const;
	Float Get_BeamLength() const;
	// Aimed: how far along the aim the beam meets the floor (past its length when it never does).
	Float Get_FloorReach() const;
	// Aimed: the ground under the beam's run past its landing, out to `length` along the beam in all (m_GroundPath).
	void Trace_GroundPath(Float length);
	// Aimed: the drawn widths grow with the beam's width over the swept beams' BeamWidth.
	Float Get_WidthScale() const { return m_Aimed ? m_AimedWidth / BeamWidth : 1.f; }
	void Hit_Player();
	void Hit_PlayerAimed();
	void Stop_ShotSound();
	void Add_Draw(const Char* pass, uint32 image, uint32 first, uint32 end);

private:
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	ComPtr<ID3D11Buffer> m_VertexBuffer{};
	Vector3 m_Center{};
	Float m_StartYaw{};
	Float m_Age{ -1.f };		// below zero while no laser is shown
	Float m_HitCooldown{};
	Bool m_Aimed{ false };
	Vector3 m_AimDirection{};	// aimed: the beam leaves m_Center this way
	Float m_FloorY{};			// aimed: the floor it burns down to
	Float m_AimedWidth{ BeamWidth };	// aimed: what hurts
	Float m_AimedLength{};		// aimed: down to the floor and on along it
	vector<Vector3> m_GroundPath{};	// aimed: points on the ground the beam runs over, from where it lands
	uint32 m_ShotSound{};		// the beams' sound while they fire (0: none)
	struct DRAW { const Char* pass; uint32 image, first, count; };
	array<DRAW, 4> m_Draws{};	// haze, then the guide lines or the beams' glow and core, then the lightning
	uint32 m_NumDraws{};

	// Timeline (seconds): guide lines, the beams shooting out, one full sweep, then the beams fade.
	static constexpr Float WarnTime = 2.5f;
	static constexpr Float GuideGrowTime = 0.5f;
	static constexpr Float FireGrowTime = 0.25f;
	static constexpr Float SweepTime = 7.f;
	static constexpr Float FadeTime = 0.3f;
	// A brief glitch as the beams shoot out, fading independently of the beam lifetime.
	// Tuned on screen.
	static constexpr Float FireGlitch = 0.7f;
	static constexpr Float FireGlitchTime = 0.4f;

	// Sound: the warning on Fire, then the beams' shot from when they fire until they are gone. Same level and
	// distances as the boss's own SoundEmitter.
	static constexpr Float SoundVolume = 0.3f;
	static constexpr Float SoundNear = 10.f;
	static constexpr Float SoundFar = 80.f;

	static constexpr uint32 NumBeams = 16;
	static constexpr Float InnerRadius = 2.f;		// the beams leave from the skirt's edge
	static constexpr Float BeamLength = 30.f;
	static constexpr Float BeamWidth = 0.7f;		// what hurts
	static constexpr Float BeamHeight = 0.3f;		// above the floor the boss stands on

	// Aimed: the muzzle's charge before the beam fires.
	static constexpr Float AimedChargeTime = 0.8f;
	static constexpr Float PlayerHeight = 1.6f;		// aimed: the player's body as a segment up from the feet
	static constexpr uint32 MaxGroundSteps = NumBeams - 1;	// aimed: pieces of the run over the ground (drawn as beams)
	static constexpr Float GroundStep = 2.f;		// aimed: the ground is probed this often at most
	static constexpr Float GroundProbe = 6.f;		// aimed: how far above the last point a probe starts
	static constexpr Float WallSlope = 1.5f;		// aimed: a rise steeper than this per metre stops the beam

	// A player whose feet are this high above the floor passes over the beams.
	static constexpr Float JumpClearance = 0.9f;
	static constexpr Float PlayerRadius = 0.4f;
	static constexpr Float HitInterval = 0.6f;
	static constexpr Float Damage = 20.f;

	// The guide lines and the beams are drawn without images by the SoftBeam passes (gaussian cross-section, faded far
	// end). The guide lines are thin glowing red lines lying flat just over the floor. Each beam, facing the camera, is a
	// thin laser as in the reference video (1:11-1:18): a narrow deep-red glow blended over the floor, a brighter red band
	// added over it and a hot core whose middle runs toward white.
	static constexpr Float GuideWidth = 0.18f;
	static constexpr Float GuideHeight = 0.2f;		// over the boss's feet, clear of the grass
	static constexpr Float GlowWidth = 0.9f;
	static constexpr Float BandWidth = 0.4f;
	static constexpr Float CoreWidth = 0.14f;
	static inline const Vector3 GuideTint{ 3.f, 0.05f, 0.08f };
	static inline const Vector3 GlowTint{ 0.75f, 0.03f, 0.08f };	// blended: a colour, not a brightness
	static inline const Vector3 BandTint{ 2.4f, 0.2f, 0.3f };
	static inline const Vector3 CoreTint{ 6.f, 2.f, 2.2f };

	// The sizzle (Effect_Em3000, as on EmRing): TSN 004/2 noise for the haze, 036/0..5 bolts swapped 12 times a second.
	static constexpr uint32 LightningFirst = 7;
	static constexpr uint32 LightningImages = 6;
	static constexpr uint32 HazeImage = 13;
	static constexpr Float HazeWidth = 1.8f;
	static constexpr Float HazeStrength = 1.8f;
	static constexpr Float HazeTiling = 0.5f;		// noise repeats per metre along a beam
	static constexpr Float LightningRate = 12.f;
	static constexpr Float LightningSize = 1.f;		// one bolt image's square along a beam
	static inline const Vector3 LightningTint{ 1.8f, 0.35f, 0.45f };
	static constexpr uint32 MaxBoltsPerBeam = 31;	// BeamLength / LightningSize, rounded up
	static constexpr Float GuideBoltSize = 1.2f;	// sparks along the guide lines, facing the camera
	static constexpr uint32 MaxGuideBoltsPerBeam = 27;
	static inline const Vector3 GuideBoltTint{ 2.2f, 0.2f, 0.25f };
	static constexpr Float MuzzleBoltSize = 3.5f;	// a camera-facing crackle where each beam leaves the skirt
	static constexpr Float MuzzleHazeSize = 4.f;	// the shimmer over it while the laser charges
	static constexpr Float MuzzleChargeFrom = 0.4f;	// its strength when the wind-up starts, rising to 1 when the beams fire
	static inline const Vector3 MuzzleTint{ 2.4f, 0.6f, 0.7f };

	// Per beam: the muzzle haze, the guide line and its sparks, or the haze, glow, band and core strips and the bolts; then
	// the two muzzle bolts.
	static constexpr uint32 VertexCapacity = NumBeams * (std::max(2 + MaxGuideBoltsPerBeam, 4 + MaxBoltsPerBeam) + 2) * 6;

public:
	static Shared<EmLaser> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
