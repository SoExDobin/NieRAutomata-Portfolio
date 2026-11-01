#pragma once
#include "PartObject.h"
#include "Em8000.h"

NS_BEGIN(Engine)
class SphereCollider;
class SoundEmitter;
NS_END

NS_BEGIN(Client)

class EmLaser;
class MonsterAOE;

// em8000's model. Its clips time the attacks as the original seq tracks do: the blades' and the body's hit volumes (the
// owner's MonsterAOE children), the rings the stomps send over the floor, the jump press's shockwave, the eye laser. A
// broken plate of armour is no longer drawn.
class CLIENT_DLL Em8000Body final : public PartObject
{
public:
	explicit Em8000Body();
	explicit Em8000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em8000Body(const Em8000Body& rhs);
	~Em8000Body() override = default;

public:
	const TRANSFORM_FRAME& Get_ModelTransform() const;
	Shared<Model> Get_ModelComponent() { return m_Model; }
	void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop);
	uint32 Get_CurrentAnimationIndex() const;
	Bool Is_AnimationFinished() const;

public:
	// Where a plate of armour is in the world as the model stands now.
	Vector3 Get_ArmourPosition(uint32 index) const;
	// The plate's meshes are no longer drawn; it bursts off with a blast and the armour sound.
	void Break_Armour(uint32 index);
	// After 0540 has thrown the plates off, none is drawn any more.
	void Hide_Armour();
	// 0700: the eye laser charges, then burns down to the floor where the player stood, and sweeps round with the body's
	// turn in 0701 until End_Laser (0702).
	void Begin_Laser();
	void End_Laser();
	// A sound held where the boss is while a looping clip runs (the hover, the dizzy chirp); a notify on the clip would
	// start it again every lap.
	void Play_LoopSound(const wstring& soundTag);
	void Stop_LoopSound();
	// The machine's running hum (bank em8000-0325-event, no clip of its own), held under every other sound from the
	// jump down until Power_Down, which cuts it with the wind-down (em8000-0283-event).
	void Start_Hum();
	void Power_Down();
	// A one-off sound from the body where its clips' own sounds come from.
	void Play_Sound(const wstring& soundTag) const;
	// 0721 / 0704: the centre gun fires `pellets` orbs spread over `spread` degrees round where its barrel points (flat).
	void Fire_Gun(uint32 pellets, Float spread);
	// The ball's spin (054C): `count` orbs from the eye (bone16), each at random inside a cone of ConeShotAngle round the
	// way the eye faces out of the body, level (it turns with the spin); the volley cannot be cut when `permanent`.
	void Fire_Cone(uint32 count, Bool permanent);
	// Stops everything an interrupted attack left running (dizzy spell, death).
	void End_Attacks();
	// The death's chain of blasts: one with the common death boom at a bone picked at random over the whole model, or at
	// the body's centre.
	void Burst(Float size, Bool atBody);
	// The body's centre (bone3) in the world: the ball's centre once it has curled up.
	Vector3 Get_BodyPosition() const { return Get_BonePosition(m_BodyBoneIndex); }
	// The ball's own spheres are out to hit (rolling, jumping, spinning).
	Bool Is_BallAttacking() const;
	// Raises the whole model over the boss's feet (054A's jump goes higher than the clip's own).
	void Set_Lift(Float height);
	// The tell as an attack starts (EmTelegraph): the machine's warning (the common tele_l), the eye flaring and the
	// streak through it.
	void Telegraph();

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();
	Vector3 Get_BonePosition(int32 boneIndex) const;
	void Hide_Plate(uint32 index);
	// A blast and the armour sound where a plate still on is (Break_Armour, and 0540 throwing the plates off).
	void Burst_Plate(uint32 index) const;
	void Set_Attack(Weak<MonsterAOE> attack, Bool active) const;
	// One ring spreading over the floor from center (EmRing GROUND), or the shockwave filling it out to endRadius (AREA).
	void Spread_Ring(const Vector3& center, Float endRadius, Bool filled = false) const;
	// Metal thrown up where a blow comes down (SparkEffect impact): under the bone on the floor, or at the bone itself,
	// `ahead` metres further along the boss's look.
	void Throw_Sparks(int32 boneIndex, Float impact, Bool onFloor = true, Float ahead = 0.f) const;
	// A crackle of electricity (SparkEffect electric) at a bone picked at random, every so often while it is alive.
	void Update_Crackle(Float timeDelta);
	// The laser's way out of the eye: level out from the body's centre through the eye (so it turns with the pose),
	// tipped down by m_LaserDrop.
	Vector3 Get_LaserDirection() const;
	// Turns the eye (bone16) on this frame's pose to look at the player's chest, at most EyeTrackAngle off the clip's
	// facing and EyeTrackSpeed degrees a second; it eases back to the clip's facing with no player, dead or while the
	// laser (which leaves along the pose) burns.
	void Track_Eye(Float timeDelta);

private:
	int32					m_RootBoneIndex{ -1 };
	int32					m_BodyBoneIndex{ -1 };
	int32					m_EyeBoneIndex{ -1 };
	array<int32, Em8000::NumArmour> m_ArmourBoneIndices{};
	vector<Bool>			m_HiddenMeshes{};
	Shared<Shader>			m_Shader{ nullptr };
	Shared<Model>			m_Model{ nullptr };
	Shared<SphereCollider>	m_HitBox{ nullptr };
	Shared<SoundEmitter>	m_SoundEmitter{ nullptr };
	Weak<MonsterAOE>		m_BladeAttacks[2]{};
	Weak<MonsterAOE>		m_PressAttack{};
	array<Weak<MonsterAOE>, Em8000::NumTailSpheres> m_TailAttacks{};
	Weak<MonsterAOE>		m_StabAttack{};		// the tail's drill in 0556
	Weak<MonsterAOE>		m_BallAttack{};		// the ball rolling, jumping and spinning into the player
	Weak<MonsterAOE>		m_BallLandAttack{};	// 054A's landing
	Weak<MonsterAOE>		m_SlamAttack{};		// 0530's box where the tail lands
	array<Bool, Em8000::NumArmour> m_PlateHidden{};
	int32					m_GunBoneIndex{ -1 };		// bone32, the centre gun's breech
	int32					m_MuzzleBoneIndex{ -1 };	// bone33, its barrel
	// Where the blows land: the blades' tips (bone304, bone560), the tail where 0530 strikes the floor (seq SE bone168)
	// and the drill at its end (bone175).
	array<int32, 2>			m_BladeBoneIndices{ -1, -1 };
	int32					m_TailBoneIndex{ -1 };
	int32					m_TailTipBoneIndex{ -1 };
	Weak<EmLaser>			m_Laser{};
	Float					m_LaserDrop{};		// the beam's fall per metre out, set when it starts
	uint32					m_LoopSound{};
	uint32					m_HumSound{};
	Float					m_TellAge{ -1.f };	// since the last Telegraph, < 0 when none is running
	Float					m_CrackleWait{};	// until the next crackle of electricity somewhere on the body
	Float					m_CrackleSnapWait{};	// until a crackle next snaps
	Vector3					m_EyeFacing{};		// the lens's facing in bone16's own space: out from the body's centre in the bind pose
	Quaternion				m_EyeTurn{};		// the eye's turn on top of the clip's, in model space

	// The plates' meshes (each a pair: the plate and its rim), in Em8000's armour order.
	static inline const Char* ArmourMeshes[Em8000::NumArmour] = {
		"RightlegParts_000", "RightlegParts_010", "RightlegParts_020",
		"LeftlegParts_000", "LeftlegParts_010", "LeftlegParts_020",
	};
	static constexpr Float BodyRadius = 3.9f;		// the sphere around bone3 (the body mesh spans 3.7 m from it)
	static constexpr Float RingSpreadSpeed = 12.f;
	static constexpr Float AreaSpreadSpeed = 160.f;	// the shockwave's rim is out at PressAreaRadius 26 in 0.16 s
	static constexpr Float RingDamage = 25.f;
	static inline const Vector3 RingTint{ 1.f, 0.45f, 0.2f };	// the orange rings of the reference video (0:59-1:07)
	// Thicker than Em3000's (0.6 / 2.2) with a heat haze round them, on request (2026-10-16); GROUND's hit band is as wide.
	static constexpr Float RingBandWidth = 1.5f;
	static constexpr Float RingGlowWidth = 4.f;
	static constexpr Float RingHaze = 1.f;
	// The shockwave (jump press, intro landing) lights no floor; it bursts up blue out of where the body lands under a big
	// flare, as in the reference (2:15, 2:33), on request (2026-10-16, data010 effect 336 and 300): puffs about AreaFlame
	// metres across, an AreaFlash metres flare. Every ring glows (RENDERGROUP::EMISSIVE). Tuned on screen.
	static constexpr Float AreaFlame = 5.f;
	static constexpr Float AreaFlash = 20.f;
	// The laser lands where the player stood when it started, within these reaches (EmInfo laser MIN 2000 MAX 4000).
	static constexpr Float LaserMinReach = 12.f;
	static constexpr Float LaserMaxReach = 30.f;
	static constexpr Float BreakBlastSize = 6.f;
	// The centre gun's orbs (em8000AtkInfo is the machine gun's: RADIUS_30 m; the damage is the other bosses' orbs').
	static constexpr Float GunShotSpeed = 18.f;
	static constexpr Float GunShotDamage = 7.f;
	static constexpr Float GunShotRange = 40.f;
	static constexpr Float MuzzleReach = 1.f;		// on past bone33 to the barrel's mouth
	// The spin's cone of orbs (on request, 2026-10-24): slower than the gun's so the gaps can be run through.
	static constexpr Float ConeShotSpeed = 10.f;
	static constexpr Float ConeShotRange = 35.f;
	static constexpr Float ConeShotAngle = 20.f;	// degrees off the eye's facing at most
	// The sparks' size by blow (SparkEffect impact): a toe or a blade, the tail, the drill's stab in the air, and the
	// whole body landing (the jump press, the ball's press, the intro).
	static constexpr Float FootImpact = 1.f;
	static constexpr Float TailImpact = 1.5f;
	static constexpr Float StabImpact = 0.5f;
	static constexpr Float BodyImpact = 2.5f;
	static constexpr Float SparkLift = 0.1f;		// over the floor, so the flash is not cut by it
	// The blades' and the tail's sparks this far ahead of the bone along the look, where the blow meets the floor in
	// front of it (on request, 2026-10-16). Tuned on screen.
	static constexpr Float HitSparkAhead = 2.f;
	// Electricity (orange like the stomp rings) crackles somewhere on it the whole fight (on request, 2026-10-16; the
	// reference's sparks round it): one every CrackleMinWait-CrackleMaxWait seconds at a bone picked at random, CrackleMinSize-CrackleMaxSize
	// metres across, up to CrackleSpread off the bone. Tuned on screen.
	static constexpr Double CrackleMinWait = 0.04;
	static constexpr Double CrackleMaxWait = 0.1;
	static constexpr Double CrackleMinSize = 1.2;
	static constexpr Double CrackleMaxSize = 2.6;
	static constexpr Double CrackleSpread = 0.6;
	// Every CrackleSnapMinWait-CrackleSnapMaxWait seconds a crackle bends the screen locally (SparkEffect refract).
	static constexpr Double CrackleSnapMinWait = 0.4;
	static constexpr Double CrackleSnapMaxWait = 1.2;
	// Rings and the shockwave briefly glitch the screen as they are born (EmRing glitch).
	static constexpr Float RingGlitch = 0.5f;
	static constexpr Float AreaGlitch = 0.8f;
	static constexpr Float TellSize = 3.f;			// the lens is far bigger than a small machine's eyes: the streak sits further out
	static constexpr Float EyeTargetHeight = 1.f;	// the player's chest over their feet

public:
	static Shared<Em8000Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
