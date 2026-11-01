#pragma once
#include "Monster.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class Em3004Body;
class EmMovement;

// The puppet Em3000 summons in its phase 2 scream (StateEm3000_Puppet): it drops from above with a few pieces of rubble,
// then stands hanging from its strings and fires machine-gun bursts at the player until it is destroyed. It never walks,
// turns or gets pushed: it keeps the way it was dropped facing (the player) and fires only at a target in front of it.
// em3004.model stacks six puppets (And_00~05) and seven pieces of rubble on one skeleton; each puppet shows one of the
// six at random with the rubble it falls with (Em3004Body).
class CLIENT_DLL Em3004 final : public Monster
{
public:
	typedef struct tagEm3004Desc : public MONSTER_DESC
	{
	} EM3004_DESC;

	// State tags of the puppet state machine.
	enum class EM3004_STATE
	{
		FALL,
		ATTACK,
		HIT,
		DEAD,
	};

	// Clip indices of em3004_Gameplay.json (Em3004.AnimationState): the 0x00 set, played by every puppet.
	enum class EM3004_ANIM
	{
		IDLE = 0,			// 0000
		FALL = 1,			// 0001 drops 8 m, lands at frame 17 (seq 0.15 s em3004_fall_impact)
		GET_UP = 2,			// 0002
		SHOOT_START = 3,	// 0005
		SHOOT_LOOP = 4,		// 0006 seq shot flags at 0, 0.25 and 0.5 s
		SHOOT_END = 5,		// 0007
		HANG = 6,			// 000e
		FLINCH = 7,			// 000f
	};

public:
	explicit Em3004();
	explicit Em3004(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3004(const Em3004& rhs);
	~Em3004() override = default;

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
	void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
	void OnDeath() override;

public:
	Bool Change_State(EM3004_STATE state);
	Bool Is_State(EM3004_STATE state) const;
	// Blows the puppet up at once (its summoner died).
	void Burst();
	// FALL's first frame holds the puppet this far above its feet; the clip lands it on them.
	static constexpr Float FallClipDrop = 8.f;
	// The hanging puppet's head, above its feet.
	static constexpr Float HeadHeight = 2.9f;

public:
	// Inspector: em3004AtkInfo TYPE_MACHINEGUN. Damage per bullet, bullet speed (m/s, SPD_7), shots before it reloads
	// (RELOAD_NUM 10) and the reload pause in seconds (RELOAD_WAIT 360 frames).
	Float Get_ShotDamage() const { return m_ShotDamage; }
	void Set_ShotDamage(Float damage) { m_ShotDamage = damage; }
	Float Get_ShotSpeed() const { return m_ShotSpeed; }
	void Set_ShotSpeed(Float speed) { m_ShotSpeed = speed; }
	int32 Get_ShotsPerReload() const { return m_ShotsPerReload; }
	void Set_ShotsPerReload(int32 shots) { m_ShotsPerReload = shots; }
	Float Get_ReloadWait() const { return m_ReloadWait; }
	void Set_ReloadWait(Float seconds) { m_ReloadWait = seconds; }

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Ready_PartObjects();
	HRESULT Ready_Components();

private:
	Shared<Em3004Body> m_MainBody{ nullptr };
	Shared<EmMovement> m_EmMovement{ nullptr };
	Shared<SphereCollider> m_PhysicalZone{ nullptr };
	// The original's POWER 70 is on its own damage scale; 12 is what em0010's machine gun (same SE) does to 2B here.
	Float m_ShotDamage{ 12.f };
	Float m_ShotSpeed{ 7.f };
	int32 m_ShotsPerReload{ 10 };
	Float m_ReloadWait{ 6.f };

public:
	static Shared<Em3004> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
