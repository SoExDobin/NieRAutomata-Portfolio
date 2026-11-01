#include "pch.h"
#include "MonsterHitBox.h"

#include <SpdLogger.h>
#include <SphereCollider.h>

#include "Model.h"

MonsterHitBox::MonsterHitBox() : PartObject{} {}
MonsterHitBox::MonsterHitBox(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{ device, context } {}
MonsterHitBox::MonsterHitBox(const MonsterHitBox& rhs)
	: PartObject{ rhs } {}

HRESULT MonsterHitBox::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT MonsterHitBox::Initialize(void* arg)
{
	if (!arg || FAILED(PartObject::Initialize(arg)))
		return E_FAIL;

	const MONSTER_HITBOX_DESC& desc = *static_cast<MONSTER_HITBOX_DESC*>(arg);
	m_TargetModel = desc.model;
	if (const Shared<Model> model = m_TargetModel.lock())
		m_TargetBoneIndex = model->Get_BoneIndexByName(desc.targetBoneName);
	if (m_TargetBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find MonsterHitBox Bone");
		return E_FAIL;
	}

	SphereCollider::SPHERE_COLLIDER_DESC sphereDesc{};
	sphereDesc.radius = desc.radius;
	sphereDesc.offset = desc.offset;
	m_HitCollider = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &sphereDesc);
	return m_HitCollider ? S_OK : E_FAIL;
}

void MonsterHitBox::Late_Update(Float timeDelta)
{
	const Shared<Model> model = m_TargetModel.lock();
	if (!model) return;

	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_HitCollider->Update(model->Get_BoneMatrix(m_TargetBoneIndex) * m_CombinedWorldMatrix);
}

Shared<MonsterHitBox> MonsterHitBox::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<MonsterHitBox>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> MonsterHitBox::Clone(void* arg)
{
	auto instance = make_shared<MonsterHitBox>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
