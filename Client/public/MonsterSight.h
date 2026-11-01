#pragma once
#include "PartObject.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL MonsterSight final : public PartObject
{
public:
	typedef struct tagMonsterSightDesc : public PARTOBJECT_DESC
	{
		Float radius{};
		Vector3 offset{};
	} MONSTER_SIGHT_DESC;

public:
	explicit MonsterSight();
	explicit MonsterSight(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit MonsterSight(const MonsterSight& rhs);
	~MonsterSight() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Enable() override;
	void On_Disable() override;

private:
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;

private:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	Shared<SphereCollider> m_Sight{ nullptr };

public:
	static Shared<MonsterSight> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
	
};


NS_END
