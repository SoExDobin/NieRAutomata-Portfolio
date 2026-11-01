#include "pch.h"
#include "StateEm0000.h"

#include "Em0000.h"
#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"

StateEm0000::StateEm0000(const wstring& tag, const Shared<Em0000>& owner)
	: State(tag), m_Owner(owner)
{
}

StateEm0000::~StateEm0000()
{
	m_Owner.reset();
	m_Body.reset();
	m_Movement.reset();
	m_States.reset();
}

HRESULT StateEm0000::Initialize()
{
	if (m_Owner.expired()) return E_FAIL;

	m_Body = m_Owner.lock()->Get_Body();
	m_Movement = m_Owner.lock()->Get_Component<Em0010Movement>();
	m_States = m_Owner.lock()->Get_Component<MonsterStateMachine>();

	return S_OK;
}

Float StateEm0000::Get_DistanceToTarget() const
{
	return m_Owner.expired() ? FLT_MAX : m_Owner.lock()->Get_DistanceToTarget();
}

Vector3 StateEm0000::Get_DirectionToTarget() const
{
	return m_Owner.expired() ? Vector3::Zero : m_Owner.lock()->Get_DirectionToTarget();
}

Bool StateEm0000::Has_Target() const
{
	return !m_Owner.expired() && m_Owner.lock()->Has_Target();
}

Bool StateEm0000::Is_TargetFront() const
{
	return !m_Owner.expired() && m_Owner.lock()->Is_TargetFront();
}

void StateEm0000::Change_ToEngageState()
{
	using msState = MonsterStateMachine::MONSTER_STATE;
	m_States.lock()->Change_State(Has_Target() ? msState::AWAIT : msState::IDLE);
}

void StateEm0000::Hold_Position(const Vector3& lookDirection)
{
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = false;
	moveData.canRotation = lookDirection.LengthSquared() > 0.f;
	moveData.lookDirection = lookDirection;
	m_Movement.lock()->Set_MovementData(moveData);
}
