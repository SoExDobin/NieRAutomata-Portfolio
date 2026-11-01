#include "pch.h"
#include "MonsterAOE.h"

#include <OBBCollider.h>
#include <SpdLogger.h>
#include <SphereCollider.h>

#include "Model.h"

MonsterAOE::MonsterAOE() : PartObject {} {}
MonsterAOE::MonsterAOE(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : PartObject{ device, context } {}
MonsterAOE::MonsterAOE(const MonsterAOE& rhs)
	: PartObject{rhs} {}

HRESULT MonsterAOE::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"MonsterAttack");

	return PartObject::Initialize_Prototype();
}

HRESULT MonsterAOE::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed To Ready_Components : MonsterAOE");
		return E_FAIL;
	}

	MONSTER_AOE_DESC& desc = *static_cast<MONSTER_AOE_DESC*>(arg);

	m_TargetModel = desc.model;
	m_BoneName = desc.targetBoneName;
	m_TargetBoneIndex = m_TargetModel.lock()->Get_BoneIndexByName(m_BoneName);
	m_DamageInfo = desc.dmgInfo;

	if (desc.extents != Vector3::Zero)
	{
		OBBCollider::OBB_COLLIDER_DESC boxColDesc{};
		boxColDesc.offset = desc.offset;
		boxColDesc.extents = desc.extents;
		boxColDesc.rotation = desc.rotation;
		m_AttackCollider = Add_Component<OBBCollider>(ETOI(LEVEL::STATIC), &boxColDesc);
	}
	else
	{
		SphereCollider::SPHERE_COLLIDER_DESC sphereColDesc{};
		sphereColDesc.radius = desc.radius;
		sphereColDesc.offset = desc.offset;
		m_AttackCollider = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &sphereColDesc);
	}
	if (nullptr == m_AttackCollider)
		return E_FAIL;

	return S_OK;
}

void MonsterAOE::On_Destroy()
{

	PartObject::On_Destroy();
}

void MonsterAOE::Priority_Update(Float timeDelta)
{
	
}

void MonsterAOE::Update(Float timeDelta)
{

}

void MonsterAOE::Late_Update(Float timeDelta)
{
	if (!m_AttackCollider->Is_Active() || m_TargetModel.expired() || m_TargetBoneIndex == -1)
		return;

	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	Matrix boneMatrix = m_TargetModel.lock()->Get_BoneMatrix(m_TargetBoneIndex);
	m_AttackCollider->Update(boneMatrix * m_CombinedWorldMatrix);
}

void MonsterAOE::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT MonsterAOE::Render()
{
	return S_OK;
}

void MonsterAOE::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void MonsterAOE::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!m_AttackCollider->Is_Active()) return;
	auto target = targetCollider->Get_Owner();
	
	if (target->Get_LayerMask().Get_LayerName() != L"Player") return;
	const RuntimeObjectId targetID = target->Get_RuntimeObjectId();

	if (!m_HitEntities.contains(targetID))
	{
		m_HitEntities.insert(targetID); 

		auto entity = target;
		if (target->Get_GameObjectType() == GAMEOBJECTTYPE::PART)
		{
			entity = static_pointer_cast<PartObject>(target)->Get_Owner();
		}

		if (auto player = static_pointer_cast<Entity>(entity))
		{
			player->TakeDamage(m_DamageInfo);

			if (false == m_DamageInfo.attacker.expired()) {
				auto attackerEntity = static_pointer_cast<Entity>(m_DamageInfo.attacker.lock());
				attackerEntity->OnAttackHit(player);
				// attackerEntity->Add_HitLag(0.05f); // Optional hit lag for monster
			}
		}
	}
}

void MonsterAOE::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void MonsterAOE::Active_Attack()
{
	m_AttackCollider->Set_Active(true);
	m_HitEntities.clear();
}

void MonsterAOE::DeActive_Attack()
{
	m_AttackCollider->Set_Active(false);
	m_HitEntities.clear();
}

Bool MonsterAOE::Is_Attacking() const
{
	return m_AttackCollider && m_AttackCollider->Is_Active();
}


Shared<MonsterAOE> MonsterAOE::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<MonsterAOE>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : MonsterAOE");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> MonsterAOE::Clone(void* arg)
{
	auto instance = make_shared<MonsterAOE>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : MonsterAOE");
		return nullptr;
	}

	return instance;
}


