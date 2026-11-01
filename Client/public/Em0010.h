#pragma once
#include "Monster.h"
#include "Em0010Body.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class Em0010Body;
class Em0010Movement;

class CLIENT_DLL Em0010 final : public Monster
{
public:
	explicit Em0010();
	explicit Em0010(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em0010(const Em0010& rhs);
	~Em0010() override;

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
	const TRANSFORM_FRAME& Get_BodyModelTransform() const override
	{
		return m_MainBody->Get_ModelTransform();
	}
	void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
	void Launch_Up(Float height) override;
	void Slam_Down(const Shared<GameObject>& plunger) override;
	void OnDeath() override;

public:
	// Gun-arm variant (Body ModelTag em0010_Gun): shoots from range and keeps a longer distance.
	Bool Get_Ranged() const { return m_Ranged; }
	void Set_Ranged(Bool ranged) { m_Ranged = ranged; }

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Ready_PartObjects();
	HRESULT Ready_Components();

private:
	Shared<Em0010Body> m_MainBody{nullptr};
	Shared<Em0010Movement> m_Em0010Movement{nullptr};
	Shared<SphereCollider> m_PhysicalZone{nullptr};
	Bool m_Ranged{ false };

public:
	static Shared<Em0010> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
	void Apply_PushoutCorrection(const Vector3& correction) override;
	void OnAttackHit(const Shared<GameObject>& target) override;

	enum class EM0010_STATE
	{
		IDLE = 0,
		WALK_F = 1, WALK_B = 2,
		WALK_L = 3, WALK_R = 4,
		WALK_LOOK_TARGET_L = 5, WALK_LOOK_TARGET_R = 6,
		WALK_END = 7, 

		SPRINT_1 = 9, SPRINT_2 = 10,
		START_SPRINT_1 = 12, START_SPRINT_2 = 13,
		START_SPRINT_3 = 14, SPRINT_END = 15,

		ESCAPE_BACK = 20,	// 0x0041

		HIT_FRONT_1 = 23, HIT_FRONT_2 = 24,
		HIT_FRONT_3 = 25,
		HIT_BACK_1 = 26, HIT_BACK_2 = 27,
		HIT_BACK_3 = 28,
		BACK_DROP_1 = 42, BACK_DROP_2 = 43,
		BACK_DROP_3 = 44,
		AIR_LAUNCH = 58,   // DM_UPPER a0170
		AIR_HOVER = 59,    // DM_UPPER a0171 loop
		AIR_LAND = 60,     // DM_UPPER a0172
		AIR_REHIT = 61,    // DM_UPPER alternate a0173
		AIR_RECOVER = 70,  // DM_UPPER a0180

		DEAD_1 = 74,  // 0x0190
		DEAD_2 = 75,  // 0x0191
		DEAD_3 = 81,  // 0x0300 (original DM_STUN start, not a death motion)

		FOOT_ATTACK = 83,	// 0x0500
		JUMP_TO_1 = 84,		// 0x0502
		JUMP_TO_2 = 85,		// 0x0503
		JUMP_TO_3 = 86,		// 0x0504
		SWING_FRONT = 87,	// 0x0510
		PUNCH_FRONT = 88,	// 0x0512
		SWING_TWICE = 89,

		SHOOT_START = 94,	// 0x0550
		SHOOT_END = 95,		// 0x0551
		SHOOT_LOOP = 96,	// 0x0552
	};

};

/*
 /    0   | 0x0000 | idle
//    1   | 0x0010 | walk forward (W)
//    2   | 0x0011 | walk backward (S)
//    3   | 0x0012 | walk left (A)
//    4   | 0x0013 | walk right (D)
//    5   | 0x0014 | move left facing target (MOVE_LEFT)
//    6   | 0x0015 | move right facing target (MOVE_RIGHT)
//    7   | 0x001a | walk end
//    8   | 0x0020 |
//    9   | 0x0021 | sprint 1
//   10   | 0x0022 | sprint 2
//   11   | 0x0025 |
//   12   | 0x002a | start sprint 1
//   13   | 0x002b | start sprint 2
//   14   | 0x002c | start sprint 3
//   15   | 0x002f | stop sprint
//   16   | 0x0030 |
//   17   | 0x0031 |
//   18   | 0x0032 |
//   19   | 0x0033 |
//   20   | 0x0041 | escape back (backstep)
//   21   | 0x0090 |
//   22   | 0x0091 |
//   23   | 0x0100 | hit from front 1
//   24   | 0x0101 | hit from front 2
//   25   | 0x0102 | hit from front 3
//   26   | 0x0103 | hit from back 1
//   27   | 0x0104 | hit from back 2
//   28   | 0x0105 | hit from back 3
//   29   | 0x0107 |
//   30   | 0x0108 |
//   31   | 0x010a |
//   32   | 0x010b |
//   33   | 0x0110 |
//   34   | 0x0111 |
//   35   | 0x0112 |
//   36   | 0x0113 |
//   37   | 0x0114 |
//   38   | 0x0120 |
//   39   | 0x0121 |
//   40   | 0x0122 |
//   41   | 0x0123 |
//   42   | 0x0130 | back drop 1
//   43   | 0x0131 | back drop 2
//   44   | 0x0132 | back drop 3
//   45   | 0x0133 |
//   46   | 0x0134 |
//   47   | 0x0135 |
//   48   | 0x0136 |
//   49   | 0x0137 |
//   50   | 0x0138 |
//   51   | 0x0139 |
//   52   | 0x0150 |
//   53   | 0x0151 |
//   54   | 0x0152 |
//   55   | 0x0153 |
//   56   | 0x0154 |
//   57   | 0x0155 |
//   58   | 0x0170 |
//   59   | 0x0171 |
//   60   | 0x0172 |
//   61   | 0x0173 |
//   62   | 0x0174 |
//   63   | 0x0175 |
//   64   | 0x017a |
//   65   | 0x017b |
//   66   | 0x017c |
//   67   | 0x017d |
//   68   | 0x017e |
//   69   | 0x017f |
//   70   | 0x0180 |
//   71   | 0x0181 |
//   72   | 0x0182 |
//   73   | 0x0183 |
//   74   | 0x0190 | dead 1
//   75   | 0x0191 | dead 2
//   76   | 0x01a0 |
//   77   | 0x01a1 |
//   78   | 0x01a2 |
//   79   | 0x01aa |
//   80   | 0x01ac |
//   81   | 0x0300 | dead 3
//   82   | 0x0301 |
//   83   | 0x0500 | foot attack (kick)
//   84   | 0x0502 | jump to 1
//   85   | 0x0503 | jump to 2
//   86   | 0x0504 | jump to 3
//   87   | 0x0510 | swing front (arm swing)
//   88   | 0x0512 | punch front
//   89   | 0x0520 |
//   90   | 0x0522 |
//   91   | 0x0530 |
//   92   | 0x0531 |
//   93   | 0x0532 |
//   94   | 0x0550 | shoot start (gun)
//   95   | 0x0551 | shoot end
//   96   | 0x0552 | shoot loop
//   97   | 0x0553 |
//   98   | 0x0554 |
//   99   | 0x0555 |
//  100   | 0x055a |
//  101   | 0x055c |
//  102   | 0x055d |
//  103   | 0x0560 |
//  104   | 0x0561 |
//  105   | 0x0562 |
//  106   | 0x05a0 |
//  107   | 0x05a1 |
//  108   | 0x05a2 |
//  109   | 0x0600 |
//  110   | 0x0601 |
//  111   | 0x0602 |
//  112   | 0x0610 |
//  113   | 0x0611 |
//  114   | 0x0612 |
//  115   | 0x0700 |
//  116   | 0x0701 |
//  117   | 0x101a |
//  118   | 0x11aa |
//  119   | 0x11ac |
//  120   | 0x21aa |
//  121   | 0x21ac |
//  122   | 0x31aa |
//  123   | 0x31ac |
//  124   | 0x550a |
요약 통계
 */

NS_END
