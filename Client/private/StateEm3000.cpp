#include "pch.h"
#include "StateEm3000.h"

#include "Em3000Body.h"
#include "EmMovement.h"

StateEm3000::StateEm3000(const wstring& tag, const Shared<Em3000>& owner)
	: State{ tag }, m_Owner{ owner } {}

StateEm3000::~StateEm3000()
{
	m_Owner.reset();
	m_Body.reset();
	m_Movement.reset();
}

HRESULT StateEm3000::Initialize()
{
	if (m_Owner.expired()) return E_FAIL;
	m_Body = static_pointer_cast<Em3000Body>(m_Owner.lock()->Find_Child(L"Em3000Body"));
	m_Movement = m_Owner.lock()->Get_Component<EmMovement>();
	return m_Body.expired() || m_Movement.expired() ? E_FAIL : S_OK;
}

Bool StateEm3000::Has_Target() const
{
	return !m_Owner.expired() && m_Owner.lock()->Has_Target();
}

Float StateEm3000::Get_DistanceToTarget() const
{
	return m_Owner.expired() ? FLT_MAX : m_Owner.lock()->Get_DistanceToTarget();
}

Vector3 StateEm3000::Get_DirectionToTarget() const
{
	return m_Owner.expired() ? Vector3::Zero : m_Owner.lock()->Get_DirectionToTarget();
}

void StateEm3000::Change_State(STATE state) const
{
	if (const Shared<Em3000> owner = m_Owner.lock())
		owner->Change_State(state);
}

void StateEm3000::Play(ANIM anim, Bool loop, Float blend) const
{
	m_Body.lock()->Set_Animation(ETOI(anim), blend, loop);
}

Bool StateEm3000::Is_Playing(ANIM anim) const
{
	return m_Body.lock()->Get_CurrentAnimationIndex() == ETOI(anim);
}

Bool StateEm3000::Is_Finished(ANIM anim) const
{
	return Is_Playing(anim) && m_Body.lock()->Is_AnimationFinished();
}

void StateEm3000::Hold_FacingTarget() const
{
	EmMovement::EM_MOVEMENT_DATA data{};
	data.lookDirection = Get_DirectionToTarget();
	data.lookDirection.y = 0.f;
	m_Movement.lock()->Set_MovementData(data);
}

void StateEm3000::Follow_RootMotion(Bool steerToTarget) const
{
	EmMovement::EM_MOVEMENT_DATA data{};
	data.isAttack = true;
	data.canRotation = steerToTarget;
	data.direction = Get_DirectionToTarget();
	data.direction.y = 0.f;
	m_Movement.lock()->Set_MovementData(data);
}
