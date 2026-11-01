#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class CLIENT_DLL Entity abstract : public GameObject
{
public:
	typedef struct tagEntityObject : public GAMEOBJECT_DESC
	{
		
	} ENTITY_DESC;

public:
	typedef struct tagDamageInfo
	{
		Weak<GameObject> attacker{};
		ATK_TYPE	attackType{ ATK_TYPE::END };
		Float		damage{ 0.f };
		Float		groggyWeight{ 0 };
		Float		knockbackForce{ 1.f };
		Bool		blowAway{ false };	// a killing blow throws the body back (shockwave, pod laser, heavy sword)

		Vector3		hitPosition{ 0.f, 0.f, 0.f };
		Quaternion	hitRotation{ 0.f, 0.f, 0.f, 1.f };	// +Y is the way hit sparks fly (sword swing, bullet bounce)

	} DAMAGE_INFO;

public:
	explicit Entity();
	explicit Entity(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Entity(const Entity& rhs);
	virtual ~Entity() override = default;

public:
	virtual void Apply_PushoutCorrection(const Vector3& correction) {};
	virtual void OnAttackHit(const Shared<GameObject>& target) {};

public: /* Entity interface */
	virtual void TakeDamage(const DAMAGE_INFO& dmgInfo);
	virtual void OnDeath() {};
	virtual Vector3 PushoutDelta(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider, Float ratio = 1.f);
	virtual void Pullout(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider, Float ratio = 1.f);

public:
	Float Get_HP() const { return m_Hp; }
	Float Get_MaxHP() const { return m_MaxHp; }
	Bool Is_Static() const { return m_IsStatic; }
	Bool Is_Dead() const { return m_Hp <= 0.f; }
	Bool Is_Invincible() const { return m_IsInvincible; }
	void Set_Invincible(Bool isInvincible) { m_IsInvincible = isInvincible; }
	const DAMAGE_INFO& Get_LastDamageInfo() { return m_LastDamageInfo; }

public: // Utility
	Bool Calc_Penetration(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider, Vector3& outDir, Float& outDepth);
	Float Get_RadiusByColliderType(const Shared<Collider>& collider, const Vector3 colDirection);

protected:
	Float m_Hp{ 1000.f };
	Float m_MaxHp{ 1000.f };
	Bool m_IsInvincible{ false };
	Bool m_IsStatic{ false };


	DAMAGE_INFO m_LastDamageInfo{};

public:
	virtual Shared<GameObject> Clone(void* arg) PURE;
};

NS_END
