#pragma once
#include "Monster.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class Em8000Body;
class EmMovement;
class MonsterHitBox;

/* The factory goliath (em8000: the sphere body on two three-toed legs, with blades and a gun) as the City Ruins waterfall
   boss. It comes down from a building beside the arena (INTRO) and fights with the patterns of the reference fight up to
   1:10, without the energy shield: the blade scissors and slam in front, the jumping scissors and the jump press, the
   stomps that send rings over the floor, the eye laser swept round by a full turn of the body, and hover steps around the
   player; and with the tail swung round or slammed down ahead, the tail's drill stabbing three times (TAIL_STAB), and the
   centre gun's shotgun and spinning fire (GUN). It never flinches.
   A plate of armour sits on each of the six toes and takes its own hits; every third plate broken knocks the boss dizzy
   (GROGGY), and while dizzy its body takes GroggyDamageScale times the damage. */
class CLIENT_DLL Em8000 final : public Monster
{
public:
	// State tags of the boss state machine.
	enum class EM8000_STATE { IDLE, MOVE, ATTACK, LASER, GROGGY, DEAD, INTRO, TAIL_STAB, GUN, BALL };

	// Clip indices of em8000_Gameplay.json (Em8000.AnimationState): the em800f motions it uses, in name order up to
	// LASER_END, then those added for the intro, the tail and the gun.
	enum class EM8000_ANIM
	{
		IDLE = 0,				// 0000
		MOVE_F_START = 1,		// 0010 hover forward
		MOVE_F_LOOP = 2,		// 0011
		MOVE_F_END = 3,			// 0012
		MOVE_B_START = 4,		// 0013 hover back
		MOVE_B_LOOP = 5,		// 0014
		MOVE_B_END = 6,			// 0015
		MOVE_L_START = 7,		// 0016 hover left
		MOVE_L_LOOP = 8,		// 0017
		MOVE_L_END = 9,			// 0018
		MOVE_R_START = 10,		// 0019 hover right
		MOVE_R_LOOP = 11,		// 001a
		MOVE_R_END = 12,		// 001b
		DEAD = 13,				// 0190
		GROGGY_IN = 14,			// 0300
		GROGGY_LOOP = 15,		// 0301
		GROGGY_END = 16,		// 0302
		JUMP_PRESS = 17,		// 0500
		STOMP_A = 18,			// 0560
		STOMP_B = 19,			// 0561
		SCISSORS = 20,			// 0600
		JUMP_SCISSORS = 21,		// 0605
		BLADE_SLAM = 22,		// 0610
		LASER_START = 23,		// 0700
		LASER_LOOP = 24,		// 0701
		LASER_END = 25,			// 0702
		TAIL_SWEEP = 26,		// 0520 tail swung round
		TAIL_STAB_START = 27,	// 0555 turns to bring the tail's drill round
		TAIL_STAB_END = 28,		// 0557 turns back
		TAIL_STAB_LOOP = 29,	// 0556 one stab a lap
		SPIN_FIRE_START = 30,	// 0703
		SPIN_FIRE_LOOP = 31,	// 0704 fires as it spins
		SPIN_FIRE_END = 32,		// 0705
		SHOTGUN_START = 33,		// 0720
		SHOTGUN_LOOP = 34,		// 0721 one blast a lap
		SHOTGUN_END = 35,		// 0722
		INTRO_HANG = 36,		// 2050 hanging on the building
		INTRO_JUMP = 37,		// 2051 the jump down to the field
		TAIL_STAB_BACK = 38,	// 055a the drill brought up at the player behind
		BALL_IN = 39,			// 0540 sheds the armour and curls up into the ball
		BALL_IDLE = 40,			// 0542
		BALL_DASH_START = 41,	// 0545 winds up and sets off rolling
		BALL_DASH_LOOP = 42,	// 0546 rolls on
		BALL_DASH_TURN = 43,	// 0547 the U-turn
		BALL_DASH_END = 44,		// 0548 hops and brakes
		BALL_DIZZY = 45,		// 0320 wobbles after the U-turn dash
		BALL_PRESS = 46,		// 054a the blade type's ball jump press
		BALL_SPIN_START = 47,	// 054b
		BALL_SPIN_LOOP = 48,	// 054c spins on the spot
		BALL_SPIN_END = 49,		// 054d
		TAIL_SLAM = 50,			// 0530 the tail brought over the body and slammed down ahead
		BALL_OUT = 51,			// 0541 the ball unfolds back onto its legs (0542's pose to 0000's)
	};

	// The attacks the idle state picks: the one-clip ones the attack state plays, then the gun's two, which GUN plays.
	enum class ATTACK { SCISSORS, BLADE_SLAM, JUMP_SCISSORS, JUMP_PRESS, STOMP, TAIL_SWEEP, TAIL_SLAM, SHOTGUN, SPIN_FIRE };

	// The armour plates, in the order of 0560/0561's stomp flags (FreeArg 1-6): the right toes, then the left. Each plate
	// is skinned to its own toe bone.
	static constexpr uint32 NumArmour = 6;
	static inline const Char* ArmourBones[NumArmour] = { "bone1570", "bone1586", "bone1602", "bone1826", "bone1842", "bone1858" };
	// 0520's hit spheres (MonsterAOE children Em8000TailAoe0..): five down the tail, its tip, and one round the feet.
	static constexpr uint32 NumTailSpheres = 7;
	// 0530 AttackTrack f72-80: a box without a part (on the root) 2 m up and 14 m ahead, 2 x 2 x 10 (half extents) long
	// ahead, where the tail lands (MonsterAOE child Em8000TailSlamAoe). Its f65-75 spheres are 0520's on the tail.
	static constexpr Float SlamReach = 14.f;
	static constexpr Float SlamHeight = 2.f;
	static inline const Vector3 SlamExtents{ 2.f, 2.f, 10.f };
	// 0556 AttackTrack f10-17 on bone175: a 1.6 x 5 x 2 box (half extents) 2 m in from the drill's end along the tail, so
	// it reaches 3 m past the end and 7 m back up the tail (MonsterAOE child Em8000StabAoe).
	static constexpr Float StabInset = 2.f;
	static inline const Vector3 StabExtents{ 1.6f, 5.f, 2.f };

public:
	explicit Em8000() = default;
	explicit Em8000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em8000(const Em8000& rhs);
	~Em8000() override = default;

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
	const TRANSFORM_FRAME& Get_BodyModelTransform() const override;
	void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
	void OnDeath() override;

public:
	Bool Change_State(EM8000_STATE state);
	Bool Is_State(EM8000_STATE state) const;
	Shared<Em8000Body> Get_Body() const { return m_MainBody; }
	ATTACK Get_NextAttack() const { return m_NextAttack; }
	void Set_NextAttack(ATTACK attack) { m_NextAttack = attack; }
	Bool Is_ArmourBroken(uint32 index) const { return index < NumArmour && m_ArmourHp[index] <= 0.f; }
	// HP fell to RageHpRatio: the idle state turns it into the ball (StateEm8000_Ball) once the attack in hand ends.
	Bool Is_BallDue() const { return m_BallDue && !m_BallForm; }
	Bool Is_BallForm() const { return m_BallForm; }
	// 0540 sheds the plates still on: they take no more hits and break no dizzy spell.
	void Enter_BallForm();
	// The fight's music (Emil - Despair, cut from the OST like the level music: the file's end runs on into
	// BgmLoopStartMs). The entrance fades the level's music out as it starts and brings this in when it ends; the death
	// fades this out and the level's music back in.
	static void Start_Bgm();
	static constexpr SOUNDCHANNEL BgmChannel = SOUNDCHANNEL::CHANNEL_23;
	static constexpr Float BgmVolume = 0.32f;		// level with the boss opera (BGM_Bt_Opera at 0.5)
	static constexpr uint32 BgmLoopStartMs = 20000;
	static constexpr Float BgmFadeOutTime = 3.f;

public:
	// Inspector: the boss's HP (em8000BaseInfo HP_10000), each plate's HP, how long it stays dizzy and how much harder the
	// body is hurt meanwhile.
	Float Get_MaxHp() const { return m_MaxHp; }
	void Set_MaxHp(Float maxHp) { m_MaxHp = maxHp; m_Hp = maxHp; }
	Float Get_ArmourHp() const { return m_ArmourMaxHp; }
	void Set_ArmourHp(Float armourHp);
	Float Get_GroggyTime() const { return m_GroggyTime; }
	void Set_GroggyTime(Float seconds) { m_GroggyTime = seconds; }
	Float Get_GroggyDamageScale() const { return m_GroggyDamageScale; }
	void Set_GroggyDamageScale(Float scale) { m_GroggyDamageScale = scale; }
	// Inspector: how far the stomps' rings run, the jump press's shockwave on the floor, the eye laser's width and its
	// length (down to the floor and on along it).
	Float Get_StompRingRadius() const { return m_StompRingRadius; }
	void Set_StompRingRadius(Float radius) { m_StompRingRadius = radius; }
	Float Get_PressAreaRadius() const { return m_PressAreaRadius; }
	void Set_PressAreaRadius(Float radius) { m_PressAreaRadius = radius; }
	Float Get_LaserWidth() const { return m_LaserWidth; }
	void Set_LaserWidth(Float width) { m_LaserWidth = width; }
	Float Get_LaserLength() const { return m_LaserLength; }
	void Set_LaserLength(Float length) { m_LaserLength = length; }
	// Inspector: how fast it turns on the spot (degrees a second) to keep its tail end on the player while it shoots.
	Float Get_TailTurnSpeed() const { return m_TailTurnSpeed; }
	void Set_TailTurnSpeed(Float degrees) { m_TailTurnSpeed = degrees; }
	// Inspector: the share of MaxHp at or under which it turns into the ball for good.
	Float Get_RageHpRatio() const { return m_RageHpRatio; }
	void Set_RageHpRatio(Float ratio) { m_RageHpRatio = ratio; }
	// Inspector: how much higher than the clip's own 10 m the ball's jump press (054A) goes.
	Float Get_BallPressLift() const { return m_BallPressLift; }
	void Set_BallPressLift(Float height) { m_BallPressLift = height; }
	// Inspector: how far (degrees off the clip's pose) and how fast (degrees a second) the eye turns to keep on the player.
	Float Get_EyeTrackAngle() const { return m_EyeTrackAngle; }
	void Set_EyeTrackAngle(Float degrees) { m_EyeTrackAngle = degrees; }
	Float Get_EyeTrackSpeed() const { return m_EyeTrackSpeed; }
	void Set_EyeTrackSpeed(Float degrees) { m_EyeTrackSpeed = degrees; }

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Ready_PartObjects();
	HRESULT Ready_Components();
	// The unbroken plate whose sphere the hit landed on, or NumArmour for the body.
	uint32 Find_HitArmour(const Vector3& hitPosition) const;
	void Break_Armour(uint32 index);

private:
	Shared<Em8000Body> m_MainBody{ nullptr };
	Shared<EmMovement> m_Movement{ nullptr };
	Shared<SphereCollider> m_PhysicalZone{ nullptr };
	array<Weak<MonsterHitBox>, NumArmour> m_ArmourHitBoxes{};
	array<Float, NumArmour> m_ArmourHp{};
	uint32 m_BrokenArmour{};
	Bool m_GroggyDue{ false };		// a third plate broke; the dizzy spell starts on the next update
	Bool m_BallDue{ false };
	Bool m_BallForm{ false };
	ATTACK m_NextAttack{ ATTACK::JUMP_PRESS };

	Float m_ArmourMaxHp{ 600.f };
	Float m_GroggyTime{ 5.f };
	Float m_GroggyDamageScale{ 2.f };
	Float m_StompRingRadius{ 32.f };	// EmInfo stomp MAX_2200 was 22; farther on request (2026-10-15)
	Float m_PressAreaRadius{ 26.f };	// 0500 seq: ground boxes of half extent 13; twice that on request (2026-10-16)
	Float m_LaserWidth{ 2.f };			// on request (2026-10-15); Em3000's beams are 0.7
	Float m_LaserLength{ 80.f };
	Float m_TailTurnSpeed{ 40.f };		// "not that fast" (2026-10-15); TURNRATE_0.03 would be about 100
	Float m_RageHpRatio{ 0.3f };		// "HP 30% 이하" (2026-10-16)
	// "at least as high as pl's double jump and air attack" (2026-10-16): a double jump reaches 7.5 m from the top of the
	// first and about 15 m pressed early, where the air combo hovers; the ball's underside then clears 16 m.
	Float m_BallPressLift{ 6.f };
	// "always looking at the player" (2026-10-16); not source data.
	Float m_EyeTrackAngle{ 60.f };
	Float m_EyeTrackSpeed{ 180.f };

	static constexpr uint32 ArmourPerGroggy = 3;

public:
	static Shared<Em8000> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
