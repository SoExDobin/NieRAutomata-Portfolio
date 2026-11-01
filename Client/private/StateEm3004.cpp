#include "pch.h"
#include "StateEm3004.h"

#include "Em3004Body.h"
#include "EmMovement.h"

StateEm3004::StateEm3004(const wstring& tag, const Shared<Em3004>& owner)
	: State{ tag }, m_Owner{ owner } {}

StateEm3004::~StateEm3004()
{
	m_Owner.reset();
	m_Body.reset();
	m_Movement.reset();
}

HRESULT StateEm3004::Initialize()
{
	if (m_Owner.expired()) return E_FAIL;
	m_Body = static_pointer_cast<Em3004Body>(m_Owner.lock()->Find_Child(L"Em3004Body"));
	m_Movement = m_Owner.lock()->Get_Component<EmMovement>();
	return m_Body.expired() || m_Movement.expired() ? E_FAIL : S_OK;
}

void StateEm3004::Change_State(STATE state) const
{
	if (const Shared<Em3004> owner = m_Owner.lock())
		owner->Change_State(state);
}

void StateEm3004::Play(ANIM anim, Bool loop, Float blend) const
{
	m_Body.lock()->Set_Animation(ETOI(anim), blend, loop);
}

Bool StateEm3004::Is_Finished(ANIM anim) const
{
	const Shared<Em3004Body> body = m_Body.lock();
	return body->Get_CurrentAnimationIndex() == ETOI(anim) && body->Is_AnimationFinished();
}

void StateEm3004::Hold_Still(Bool flying) const
{
	EmMovement::EM_MOVEMENT_DATA data{};
	data.canRotation = false;
	data.followRootHeight = flying;
	m_Movement.lock()->Set_MovementData(data);
}
