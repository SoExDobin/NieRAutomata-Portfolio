#include "pch.h"
#include "Pl0000EvadeChecker.h"
#include "Pl0000.h"
#include <SpdLogger.h>
#include "SphereCollider.h"

Pl0000EvadeChecker::Pl0000EvadeChecker() : PartObject{} {}
Pl0000EvadeChecker::Pl0000EvadeChecker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{device, context} {}
Pl0000EvadeChecker::Pl0000EvadeChecker(const Pl0000EvadeChecker& rhs)
	: PartObject{rhs} {}

HRESULT Pl0000EvadeChecker::Initialize_Prototype()
{
	m_LayerMask.Set_UseCustomMask(true);
	m_LayerMask.ClearMask();
	m_LayerMask.Set_Mask(L"MonsterAttack");

	return PartObject::Initialize_Prototype();
}

HRESULT Pl0000EvadeChecker::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed To Initialize Pl0000EvadeChecker");
		return S_OK;
	}

	SphereCollider::SPHERE_COLLIDER_DESC colDesc{};
	colDesc.radius = 1.25f;
	colDesc.offset = Vector3::UnitY;
	m_EvadeChecker = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &colDesc);
	if (m_EvadeChecker == nullptr)
		return E_FAIL;

	return S_OK;
}

HRESULT Pl0000EvadeChecker::Begin()
{


	return S_OK;
}

void Pl0000EvadeChecker::Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_EvadeChecker->Update(m_CombinedWorldMatrix);
}

void Pl0000EvadeChecker::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Pl0000EvadeChecker::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	const auto pl0000 = static_pointer_cast<Pl0000>(Get_Parent());
	if (!pl0000) return;

	auto attacker = targetCollider->Get_Owner();

	if (attacker->Get_LayerMask().Get_LayerName() != L"MonsterAttack") return;
	pl0000->TryEvade(attacker);
}

void Pl0000EvadeChecker::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

Shared<Pl0000EvadeChecker> Pl0000EvadeChecker::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000EvadeChecker>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000EvadeChecker");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Pl0000EvadeChecker::Clone(void* arg)
{
	auto instance = make_shared<Pl0000EvadeChecker>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000EvadeChecker");
		return nullptr;
	}

	return instance;
}



