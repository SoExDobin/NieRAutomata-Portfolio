#pragma once
#include "Monster.h"

NS_BEGIN(Engine)
class Light;
class SphereCollider;
NS_END

NS_BEGIN(Client)

class Em3000Body;
class Em3004;
class EmMovement;

// The opera boss (Simone). Phase 1 alternates melee (front stomp, blade mixer) and ranged patterns; at half HP it
// staggers, sheds the dress parts and turns into the chasing phase 2 (jump stomps).
class CLIENT_DLL Em3000 final : public Monster
{
public:
	typedef struct tagEm3000Desc : public MONSTER_DESC
	{

	} EM3000_DESC;

	// State tags of the boss state machine.
	enum class EM3000_STATE
	{
		INTRO,
		IDLE,
		MELEE,
		RANGE,
		GROGGY,				// phase 1 -> 2 stagger
		TRANSFORM,
		CHASE,
		STOMP,
		PHASE2_GROGGY,
		PUPPET,
		DEAD,
	};

	// Clip indices of em3000_Gameplay.json (Em3000.AnimationState).
	enum class EM3000_ANIM
	{
		IDLE = 0,				// 0000
		MOVE_START = 1,			// 0010
		MOVE_LOOP_1 = 2,		// 0011
		MOVE_LOOP_2 = 3,		// 0012
		MOVE_END = 4,			// 0013
		BEFORE_GROGGY = 14,		// 0150
		GROGGY_IN = 15,			// 0300
		GROGGY_LOOP = 16,		// 0301
		GROGGY_END = 17,		// 0302
		HAND_WAVE = 20,			// 0500
		MIXER_START = 21,		// 0510
		MIXING = 22,			// 0511
		MIXER_END = 23,			// 0512
		FRONT_STOMP = 24,		// 0520
		RING_CHARGE = 25,		// 0550 soprano charge
		INTRO = 26,				// 0556
		RING_THROW = 28,		// 0560 EMP around shot
		DANMAK_START_1 = 29,	// 05a0
		DANMAK_LOOP_1 = 30,		// 05a1
		DANMAK_END_1 = 31,		// 05a2
		DANMAK_START_2 = 32,	// 05a5
		DANMAK_LOOP_2 = 33,		// 05a6
		DANMAK_END_2 = 34,		// 05a7
		DANMAK_START_3 = 35,	// 05c0
		DANMAK_LOOP_3 = 36,		// 05c1
		DANMAK_END_3 = 37,		// 05c2
		PHASE_CHANGE_1 = 38,	// 0600
		PHASE_CHANGE_2 = 39,	// 0610
		PHASE2_IDLE = 41,		// 1000
		PHASE_CHANGE_3 = 42,	// 100a
		CHASE = 43,				// 1010
		READY_CHASE = 44,		// 101a
		JUMP_STOMP = 51,		// 1500
		SCREAMING = 52,			// 1550
		HOVER_IN = 54,			// e001: hovers 9 m forward off the stage, 1.46 m down (root channel em3000)
		EVENT_INTRO = 57,		// ev0301 em3000_00_000: the entrance event on the stage, 802 frames
	};

public:
	explicit Em3000() = default;
	explicit Em3000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3000(const Em3000& rhs);
	~Em3000() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	const TRANSFORM_FRAME& Get_BodyModelTransform() const;
	void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
	Bool Deflects(const Shared<GameObject>& hitPart) const override;
	Bool Pushes_PlayerOnContact() const override { return false; }
	void OnDeath() override;

public:
	Bool Change_State(EM3000_STATE state);
	Bool Is_State(EM3000_STATE state) const;
	Bool Is_SecondPhase() const { return m_IsSecondPhase; }
	// The pink stage spots, lit when the entrance ends (the spot-on sound, reference 0:36) and kept on for the fight:
	// follow spots on the boss and on the player it found, and two fixed ones down the curtain. origin and forward: the
	// stage frame up on the stage where the boss starts the entrance.
	void Light_StageSpots(const Vector3& origin, const Vector3& forward);
	// The boss has come down to fight (reference video 0:36, the WARNING): the two curtain spots swing onto the player
	// and the boss.
	void Cast_StageSpots() { m_SpotsOnCast = true; }
	// The opera (BGM.bnk Bt_Opera, Layer_Opera=Intro_Off): from 0556 in the entrance (reference video 0:26.8, matched by
	// cross-correlating the video's sound against the layer) until the boss breaks up. Its soprano lead-in (BGM-3497-event)
	// plays on its own channel. Both channels are the opera's alone: a shared one would hand its handle to the next sound
	// and StopSound would miss the opera.
	static constexpr SOUNDCHANNEL BgmChannel = SOUNDCHANNEL::CHANNEL_31;
	static constexpr SOUNDCHANNEL BgmIntroChannel = SOUNDCHANNEL::CHANNEL_32;
	static constexpr Float BgmVolume = 0.5f;
	// Phase 2 scream (StateEm3000_Puppet): drops em3004 puppets from under the dome ceiling onto a ring around the player
	// until MaxPuppets are up.
	void Summon_Puppets();
	// Death: every puppet still up bursts with the boss.
	void Burst_Puppets();
	// Phase 2 keeps stomping while one is still up and rests once all are down (StateEm3000_Chase/Stomp).
	Bool Has_LivingPuppets() const;
	// Phase 2 recolours the stage spots (eased over about a second): deep crimson while it rages (the transformation and
	// each scream), blue once its puppets are all down. Phase 1 keeps the Inspector color.
	void Tint_StageSpots(const Color& color) { m_StageSpotTint = color; m_IsStageSpotTinted = true; }
	static constexpr Color RageSpotColor{ 0.75f, 0.02f, 0.12f, 1.f };
	static constexpr Color CalmSpotColor{ 0.2f, 0.45f, 1.f, 1.f };

public:
	// Inspector: the stage spots' color (times Intensity), cone half angles in degrees, range in meters and beam
	// scattering (1/m, 0 = no visible beam). Lit spots take edits the next frame in play.
	const Color& Get_StageSpotColor() const { return m_StageSpotColor; }
	void Set_StageSpotColor(const Color& color) { m_StageSpotColor = color; }
	Float Get_StageSpotIntensity() const { return m_StageSpotIntensity; }
	void Set_StageSpotIntensity(Float intensity) { m_StageSpotIntensity = intensity; }
	Float Get_StageSpotInner() const { return m_StageSpotInner; }
	void Set_StageSpotInner(Float degrees) { m_StageSpotInner = degrees; }
	Float Get_StageSpotOuter() const { return m_StageSpotOuter; }
	void Set_StageSpotOuter(Float degrees) { m_StageSpotOuter = degrees; }
	Float Get_StageSpotRange() const { return m_StageSpotRange; }
	void Set_StageSpotRange(Float range) { m_StageSpotRange = range; }
	Float Get_StageSpotBeam() const { return m_StageSpotBeam; }
	void Set_StageSpotBeam(Float beam) { m_StageSpotBeam = beam; }

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Ready_PartObjects();
	HRESULT Ready_Components();
	// Moves the follow spots after the boss and the player (eased, as a spot operator would) and applies the Inspector values.
	void Update_StageSpots(Float timeDelta);

private:
	Shared<Em3000Body> m_MainBody{ nullptr };
	Shared<EmMovement> m_EmMovement{ nullptr };
	Shared<SphereCollider> m_PhysicalZone{ nullptr };
	Bool m_IsSecondPhase{ false };
	vector<Shared<Light>> m_StageSpots;
	vector<Vector3> m_StageSpotAims;
	Vector3 m_StageRight{};						// the stage frame the spots hang in, the boss's at the reveal
	Vector3 m_StageForward{};
	Vector3 m_StageOrigin{};
	Weak<GameObject> m_SpotPlayer{};
	Bool m_SpotsOnCast{ false };
	vector<Weak<Em3004>> m_Puppets;
	static constexpr uint32 MaxPuppets = 4;			// the original calls four Em3004
	static constexpr Float PuppetRingRadius = 7.f;	// m from the player, spread evenly so they do not overlap
	static constexpr Float PuppetWallMargin = 1.5f;	// a ring point past a wall comes this far inside it (and steps in by it)
	static constexpr Float PuppetFloorStep = 2.f;	// a ring point's floor within this of the player's (the stage is 1.465 m up)
	static constexpr Float PuppetDropHeight = 25.f;	// m above the floor the drop starts where no ceiling is hit sooner
	static constexpr Float PuppetCeilingGap = 0.5f;	// between the ceiling and the head at the top of the drop
	Color m_StageSpotColor{ 1.f, 0.28f, 0.6f, 1.f };
	Color m_StageSpotTint{};
	Bool m_IsStageSpotTinted{ false };
	Color m_StageSpotShown{};					// what the spots show, easing to the tint (or the Inspector color)
	Float m_StageSpotIntensity{ 1.6f };
	Float m_StageSpotInner{ 7.f };
	Float m_StageSpotOuter{ 12.f };
	Float m_StageSpotRange{ 40.f };
	Float m_StageSpotBeam{ 0.02f };

public:
	static Shared<Em3000> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};

NS_END
