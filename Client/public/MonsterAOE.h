#pragma once
#include "PartObject.h"
#include "Entity.h"

NS_BEGIN(Engine)
class Collider;
class Model;
NS_END

NS_BEGIN(Client)
class Em0010Body;
class CLIENT_DLL MonsterAOE final : public PartObject
{
public:
	typedef struct tagMonsterAoeDesc : public PARTOBJECT_DESC
	{
		Entity::DAMAGE_INFO dmgInfo{};
		Weak<Model> model{};
		const Char* targetBoneName;
		Float radius{};
		Vector3 offset{};
		// Non-zero: a box with these half extents, turned by rotation (radians, as OBBCollider takes it), instead of the sphere.
		Vector3 extents{};
		Vector3 rotation{};
	} MONSTER_AOE_DESC;

public:
	explicit MonsterAOE();
	explicit MonsterAOE(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit MonsterAOE(const MonsterAOE& rhs);
	~MonsterAOE() override = default;
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
public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

public:
	void Active_Attack();
	void DeActive_Attack();
	Bool Is_Attacking() const;

private:
	Weak<Model>					m_TargetModel{};
	int32						m_TargetBoneIndex{ -1 };
	string						m_BoneName{};
	Entity::DAMAGE_INFO			m_DamageInfo{};
	Shared<Collider>			m_AttackCollider{ nullptr };
	unordered_set<RuntimeObjectId>	m_HitEntities;

	
public:
	static Shared<MonsterAOE> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};
NS_END
