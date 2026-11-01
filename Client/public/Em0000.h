#pragma once
#include "Monster.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class Em0000Body;
class Em0010Movement;

/* Small stubby machine (em0000 family; Level 4 wears the amusement-park em0003 mesh in its place). Melee only: a punch up close, an
   arm swing that lunges in, and a diving body tackle from further out. Moves with Em0010Movement like Em0010.
   Variants of the family (Emb001) wear another body, set their part prefix and reach before Initialize, and pick their
   own attacks. */
class CLIENT_DLL Em0000 : public Monster
{
public:
	explicit Em0000();
	explicit Em0000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em0000(const Em0000& rhs);
	~Em0000() override;

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
	void Launch_Up(Float height) override;
	void Slam_Down(const Shared<GameObject>& plunger) override;
	void OnDeath() override;
	void OnAttackHit(const Shared<GameObject>& target) override;
	void Apply_PushoutCorrection(const Vector3& correction) override;

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Ready_PartObjects();
	HRESULT Ready_Components();

protected:
	Shared<Em0000Body> m_MainBody{ nullptr };
	Shared<Em0010Movement> m_Movement{ nullptr };
	Shared<SphereCollider> m_PhysicalZone{ nullptr };

public:
	// Clip index = position in the sorted em000f.dat .mot list (Models/em0000/Animations, preset em0000_Gameplay).
	enum class EM0000_STATE
	{
		IDLE = 0,			// 0x0000
		WALK_F = 3,			// 0x0010
		DASH = 8,			// 0x0021 loop
		ESCAPE_BACK = 13,	// 0x0041
		HIT_FRONT = 17,		// 0x0110 DM_MIDDLE_F
		HIT_BACK = 18,		// 0x0111 DM_MIDDLE_B
		HIT_BIG_FRONT = 22,	// 0x0120 DM_BIG_F
		HIT_BIG_BACK = 23,	// 0x0121 DM_BIG_B
		BLOW_START = 26,	// 0x0130 DM_BLOW
		BLOW_LOOP = 27,		// 0x0131
		BLOW_LAND = 28,		// 0x0132
		AIR_LAUNCH = 38,	// 0x0170 DM_UPPER
		AIR_HOVER = 39,		// 0x0171 loop
		AIR_LAND = 40,		// 0x0172
		AIR_REHIT = 41,		// 0x0173
		RECOVER = 50,		// 0x0180 getting up
		DEAD = 54,			// 0x0190 DIE
		PUNCH = 65,			// 0x0500 short punch, steps 2 m in
		SWING_LUNGE = 66,	// 0x0510 arm swing lunging 0.85 m
		TACKLE_START = 67,	// 0x0520 throws itself forward 2.7 m
		TACKLE_SLIDE = 68,	// 0x0521 belly slide, 6 m in 10 frames
		TACKLE_GETUP = 69,	// 0x0522
		// Emb001 (golden rabbit statue)
		WAKE = 2,			// 0x0003 lowers the trumpet after a fanfare and squares up (emb001 battle start)
		TRUMPET_START = 81,	// 0x05b0 raises the trumpet to the mouth
		STATUE_POSE = 82,	// 0x05b1 trumpet at the mouth: the statue's held pose and the trumpet attack's blowing loop
		TRUMPET_END = 83,	// 0x05b2 lowers it
		// Level 4 park visitor (passive em0003)
		PAPER_THROW = 97,	// 0xe060 throws a fistful of paper confetti twice (em000f.eff 200, texture 125)
	};

	// Distances the AI attacks from (m, at the body's own size): up close once the target is in front within melee, and a
	// roll every second for the far attack between farMin and farMax.
	struct ATTACK_REACH
	{
		Float melee;
		Float farMin;
		Float farMax;
		Float farChance;
	};

public:
	// The first clip of the attack picked for this distance (Em0000: punch or lunging swing up close, the tackle further).
	virtual EM0000_STATE Choose_Attack(Float distance) const;
	// The reach in world metres: a body scaled up in the scene (Emb001) reaches as much further.
	ATTACK_REACH Get_AttackReach() const;
	// A statue holds its pose and ignores the player until it wakes (Emb001).
	virtual Bool Is_Dormant() const { return false; }
	// The clip it runs at the player in (Em0000 dashes; Emb001 lumbers).
	virtual EM0000_STATE Get_ChaseClip() const { return EM0000_STATE::DASH; }
	// The body's playback rate on top of its size cadence (Em0000Body::Update); Emb001 speeds its attacks up.
	virtual Float Get_AnimationRate() const { return 1.f; }
	const Shared<Em0000Body>& Get_Body() const { return m_MainBody; }
	// Child keys are this prefix + Body / Sight / LeftArm / RightArm / Torso / HpBar; the body prototype is <prefix>Body.
	const wstring& Get_PartPrefix() const { return m_PartPrefix; }

protected:
	// Set by a variant before Em0000::Initialize builds the parts.
	wstring m_PartPrefix{ L"Em0000" };
	Float m_HpBarHeight{ 1.7f };
	ATTACK_REACH m_AttackReach{ 3.5f, 5.f, 9.f, 0.35f };

public:
	static Shared<Em0000> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
