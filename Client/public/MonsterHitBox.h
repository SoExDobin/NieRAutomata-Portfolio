#pragma once
#include "PartObject.h"

NS_BEGIN(Engine)
class SphereCollider;
class Model;
NS_END

NS_BEGIN(Client)

// A sphere that takes the owner monster's hits, carried by one bone of its model. A body part holds one SphereCollider,
// so a monster whose body needs more spheres adds these (Em3000's upper body). It sits on the Monster layer like a body
// part, so attacks reach the owner through it the same way.
class CLIENT_DLL MonsterHitBox final : public PartObject
{
public:
	typedef struct tagMonsterHitBoxDesc : public PARTOBJECT_DESC
	{
		Weak<Model> model{};
		const Char* targetBoneName{};
		Float radius{};
		Vector3 offset{};		// in the bone's space
	} MONSTER_HITBOX_DESC;

public:
	explicit MonsterHitBox();
	explicit MonsterHitBox(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit MonsterHitBox(const MonsterHitBox& rhs);
	~MonsterHitBox() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Late_Update(Float timeDelta) override;

private:
	Weak<Model>					m_TargetModel{};
	int32						m_TargetBoneIndex{ -1 };
	Shared<SphereCollider>		m_HitCollider{ nullptr };

public:
	static Shared<MonsterHitBox> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
