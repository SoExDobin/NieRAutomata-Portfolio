#pragma once
#include "Entity.h"

NS_BEGIN(Client)

class MonsterStateMachine;
class HpBarWorldUI;

class CLIENT_DLL Monster abstract : public Entity
{
public:
	typedef struct tagMonsterObjectDesc : public ENTITY_DESC {} MONSTER_DESC;

public:
	explicit Monster();
	explicit Monster(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Monster(const Monster& rhs);
	virtual ~Monster() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Enable() override;
	void On_Disable() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

protected:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

public:
	void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
	// Physical overlap only; attack and damage colliders keep their own handling.
	virtual Bool Pushes_PlayerOnContact() const { return Is_Static(); }
	virtual void Launch_Up(Float height) {}
	// Driven down by 2B's air plunge while it hangs in the air, never above the plunger's feet so the two land together;
	// machines that cannot be launched ignore it.
	virtual void Slam_Down(const Shared<GameObject>& plunger) {}
	// Whether a shot that touched this part of the monster glances off instead of hurting (Em3000's skirt). Bullet asks
	// before it hits.
	virtual Bool Deflects(const Shared<GameObject>& /*hitPart*/) const { return false; }
	// This frame's root motion of the body, which Em0010Movement turns into movement (Em0010, Em0000). None by default.
	virtual const TRANSFORM_FRAME& Get_BodyModelTransform() const;

public:
	Bool Is_TargetFront() const;
	Bool Has_Target() const { return !m_TargetPlayer.expired(); }
	void Set_Target(const Shared<GameObject>& target) { m_TargetPlayer = target; }
	Shared<GameObject> Get_Target() const { return m_TargetPlayer.lock(); }
	// A park visitor (MonsterSpawner Passive): its sight never picks the player, but a hit still turns it on the
	// attacker (TakeDamage), and its idle plays the park's own business (em0000 throws paper, the em0010 gun fireworks).
	Bool Is_Passive() const { return m_Passive; }
	void Set_Passive(Bool passive) { m_Passive = passive; }
	const Shared<HpBarWorldUI>& Get_HpBarUI() const { return m_HpBarUI; }
	Vector3 Get_DirectionToTarget() const;
	Float Get_DistanceToTarget() const;

protected:
	void Play_HitSFX(const DAMAGE_INFO& dmgInfo) const;
	void DisplaySparkEffect(ATK_TYPE atkType, Vector3 position, Quaternion rotation = { 0.f, 0.f, 0.f, 1.f }) const;

protected:
	Shared<HpBarWorldUI> m_HpBarUI{ nullptr };

protected:
	Shared<MonsterStateMachine> m_States;
	Weak<GameObject> m_TargetPlayer{};
	Bool m_Passive{ false };
	uint32 m_PlayerPhysicalLayerIndex{};
	uint32 m_MonsterPhysicalLayerIndex{};

public:
	virtual Shared<GameObject> Clone(void* arg) override PURE;
};

NS_END
