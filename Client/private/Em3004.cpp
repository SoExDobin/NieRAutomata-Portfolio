#include "pch.h"
#include "Em3004.h"

#include <Game.h>
#include <SpdLogger.h>
#include <SphereCollider.h>

#include "Em3004Body.h"
#include "EmMovement.h"
#include "HpBarWorldUI.h"
#include "MonsterStateMachine.h"
#include "StateEm3004_Attack.h"
#include "StateEm3004_Dead.h"
#include "StateEm3004_Fall.h"
#include "StateEm3004_Hit.h"

Em3004::Em3004() : Monster{} {}
Em3004::Em3004(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Monster{device, context} {}
Em3004::Em3004(const Em3004& rhs)
	: Monster{ rhs }, m_ShotDamage{ rhs.m_ShotDamage }, m_ShotSpeed{ rhs.m_ShotSpeed },
	m_ShotsPerReload{ rhs.m_ShotsPerReload }, m_ReloadWait{ rhs.m_ReloadWait } {}

HRESULT Em3004::Initialize_Prototype()
{
	if (FAILED(Monster::Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Create Prototype Monster");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Em3004::Initialize(void* arg)
{
	if (FAILED(Monster::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize em3004");
		return E_FAIL;
	}
	// Under half of em0010's 2000 (em3004BaseInfo HP 10000 is on the original's scale).
	m_MaxHp = 500.f;
	m_Hp = m_MaxHp;
	// Hung from its strings, it stands its ground: whatever runs into it is pushed out instead (Pl0000::OnCollisionStay).
	m_IsStatic = true;

	if (FAILED(Ready_PartObjects()))
	{
		LOG_ERROR(L"Failed to Ready PartObjects em3004 ");
		return E_FAIL;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready Components em3004 ");
		return E_FAIL;
	}

	return S_OK;
}

void Em3004::On_Destroy()
{
	m_MainBody.reset();
	Monster::On_Destroy();
}

void Em3004::Priority_Update(Float timeDelta)
{

}

void Em3004::Update(Float timeDelta)
{
	m_States->Update_State(timeDelta);
	m_EmMovement->Update_Movement(timeDelta);
	m_Transform->Update_WorldMatrix();
	m_PhysicalZone->Update(*m_Transform->Get_WorldMatrixPtr());
}

void Em3004::Late_Update(Float timeDelta)
{

}

void Em3004::Fixed_Update(Float fixedDelta)
{

}

HRESULT Em3004::Render()
{
	return S_OK;
}

void Em3004::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

void Em3004::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	if (Is_Dead()) return;

	Play_HitSFX(dmgInfo);
	DisplaySparkEffect(dmgInfo.attackType, dmgInfo.hitPosition, dmgInfo.hitRotation);

	// Pod bullets (no groggy weight) only chip HP; a sword or the pod laser makes it flinch once it has landed.
	if (dmgInfo.groggyWeight > 0.f && !Is_State(EM3004_STATE::FALL))
		Change_State(EM3004_STATE::HIT);

	Monster::TakeDamage(dmgInfo);
}

void Em3004::OnDeath()
{
	m_MainBody->Off_Colliders();
	m_PhysicalZone->Set_Active(false);
	if (m_HpBarUI)
		m_HpBarUI->Set_Active(false);
	Change_State(EM3004_STATE::DEAD);
}

Bool Em3004::Change_State(EM3004_STATE state)
{
	return m_States->Change_StateByTag(Helper::To_wString(magic_enum::enum_name(state)));
}

Bool Em3004::Is_State(EM3004_STATE state) const
{
	const Shared<State> current = m_States ? m_States->Get_CurrentState() : nullptr;
	return current && current->IsSameTag(Helper::To_wString(magic_enum::enum_name(state)));
}

void Em3004::Burst()
{
	if (Is_Dead()) return;
	m_Hp = 0.f;
	OnDeath();
}

void Em3004::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

void Em3004::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	Monster::OnCollisionStay(ownCollider, targetCollider);
}

void Em3004::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

HRESULT Em3004::Ready_PartObjects()
{
	Em3004Body::EM3004BODY_DESC em3004BodyDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"Em3004Body", L"Em3004Body", &em3004BodyDesc)))
		return E_FAIL;
	m_MainBody = static_pointer_cast<Em3004Body>(Find_Child(L"Em3004Body"));
	if (nullptr == m_MainBody)
		return E_FAIL;

	// Over the head of the hanging puppet (its feet hang 1.2 m up, the head is near 2.9 m).
	HpBarWorldUI::HP_BAR_WORLD_UI_DESC hpBarDesc{};
	hpBarDesc.target = static_pointer_cast<Entity>(shared_from_this());
	hpBarDesc.worldOffset = Vector3{ 0.f, 3.2f, 0.f };
	hpBarDesc.anchor = UI_ANCHOR::TOP_LEFT;
	hpBarDesc.sizeX = 200.f;
	hpBarDesc.sizeY = 10.f;
	if (FAILED(Add_Child(Get_LevelIndex(), L"HpBarWorldUI", L"Em3004HpBar", &hpBarDesc)))
		return E_FAIL;
	m_HpBarUI = static_pointer_cast<HpBarWorldUI>(Find_Child(L"Em3004HpBar"));

	return S_OK;
}

HRESULT Em3004::Ready_Components()
{
	EmMovement::EM_MOVEMENT_DESC movementDesc{};
	movementDesc.velocity = Vector3{ 0.f, 0.f, 0.f };
	movementDesc.moveSpeed = 0.f;
	movementDesc.targetDirection = Vector3::Zero;
	movementDesc.turnSpeed = 5.f;
	m_EmMovement = Add_Component<EmMovement>(ETOI(LEVEL::GAMEPLAY), &movementDesc);
	if (nullptr == m_EmMovement)
		return E_FAIL;

	// StateMachine은 마지막에 처리
	if (nullptr == (m_States = Add_Component<MonsterStateMachine>(ETOI(LEVEL::GAMEPLAY))))
		return E_FAIL;
	const auto em3004 = static_pointer_cast<Em3004>(shared_from_this());
	const auto tag = [](EM3004_STATE state) { return Helper::To_wString(magic_enum::enum_name(state)); };
	if (FAILED(m_States->Add_State(StateEm3004_Fall::Create(tag(EM3004_STATE::FALL), em3004))) ||
		FAILED(m_States->Add_State(StateEm3004_Attack::Create(tag(EM3004_STATE::ATTACK), em3004))) ||
		FAILED(m_States->Add_State(StateEm3004_Hit::Create(tag(EM3004_STATE::HIT), em3004))) ||
		FAILED(m_States->Add_State(StateEm3004_Dead::Create(tag(EM3004_STATE::DEAD), em3004))))
		return E_FAIL;

	if (FAILED(m_EmMovement->Begin()))
	{
		LOG_ERROR(L"Failed to Begin EM3004 Movement");
		return E_FAIL;
	}

	// The hanging body (1.2 .. 2.9 m above the feet) pushes the player out.
	SphereCollider::SPHERE_COLLIDER_DESC physicalZoneDesc{};
	physicalZoneDesc.offset = Vector3{ 0.f, 2.f, 0.f };
	physicalZoneDesc.radius = 0.6f;
	m_PhysicalZone = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &physicalZoneDesc);
	if (nullptr == m_PhysicalZone)
		return E_FAIL;

	Change_State(EM3004_STATE::FALL);

	return S_OK;
}

Shared<Em3004> Em3004::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3004>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : EM3004");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em3004::Clone(void* arg)
{
	auto instance = make_shared<Em3004>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : EM3004");
		return nullptr;
	}

	return instance;
}
