#include "pch.h"
#include "StateEm0010.h"

#include "Em0010.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"

using Client::MonsterStateMachine;

StateEm0010::StateEm0010(const wstring& tag, const Shared<Em0010>& owner)
	: State(tag), m_Owner(owner)
{
}

StateEm0010::~StateEm0010()
{
	m_Owner.reset();
	m_Body.reset();
	m_Movement.reset();
	m_States.reset();
}

HRESULT StateEm0010::Initialize()
{
	if (m_Owner.expired()) return E_FAIL;

	m_Body = static_pointer_cast<Em0010Body>(m_Owner.lock()->Find_Child(L"Em0010Body"));

	m_Movement = m_Owner.lock()->Get_Component<Em0010Movement>();
	m_States = m_Owner.lock()->Get_Component<MonsterStateMachine>();

	return S_OK;
}

Float StateEm0010::Get_DistanceToTarget() const
{
	if (m_Owner.expired()) return FLT_MAX;
	return m_Owner.lock()->Get_DistanceToTarget();
}

Vector3 StateEm0010::Get_DirectionToTarget() const
{
	if (m_Owner.expired()) return Vector3::Zero;
	return m_Owner.lock()->Get_DirectionToTarget();
}

Bool StateEm0010::Has_Target() const
{
	if (m_Owner.expired()) return false;
	return m_Owner.lock()->Has_Target();
}

Bool StateEm0010::Is_TargetFront() const
{
	if (m_Owner.expired()) return false;
	return m_Owner.lock()->Is_TargetFront();
}

Bool StateEm0010::Is_Ranged() const
{
	if (m_Owner.expired()) return false;
	return m_Owner.lock()->Get_Ranged();
}

Vector3 StateEm0010::Get_TargetAimPosition() const
{
	// Aim at the chest; the target's position is at its feet.
	const Shared<GameObject> target = m_Owner.expired() ? nullptr : m_Owner.lock()->Get_Target();
	if (!target) return Vector3::Zero;
	return target->Get_Transform()->Get_Position() + Vector3{ 0.f, 1.f, 0.f };
}

void StateEm0010::Change_ToEngageState()
{
	using msState = MonsterStateMachine::MONSTER_STATE;

	if (Has_Target() && Get_DistanceToTarget() <= Get_KeepDistanceMax())
		m_States.lock()->Change_State(msState::AWAIT);
	else if (Has_Target())
		m_States.lock()->Change_State(msState::CHASE);
	else
		m_States.lock()->Change_State(msState::IDLE);
}

