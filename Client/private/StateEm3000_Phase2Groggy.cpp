#include "pch.h"
#include "StateEm3000_Phase2Groggy.h"

#include "EmMovement.h"

StateEm3000_Phase2Groggy::StateEm3000_Phase2Groggy(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Phase2Groggy::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_Elapsed = 0.f;
	m_Owner.lock()->Tint_StageSpots(Em3000::CalmSpotColor);
	m_Movement.lock()->Set_MovementData({});
	Play(ANIM::PHASE2_IDLE, true);
	return true;
}

void StateEm3000_Phase2Groggy::Update(Float timeDelta)
{
	m_Elapsed += timeDelta;
	if (m_Elapsed >= Duration)
		Change_State(STATE::PUPPET);
}

void StateEm3000_Phase2Groggy::StateExitInvoke()
{
}

Shared<StateEm3000_Phase2Groggy> StateEm3000_Phase2Groggy::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Phase2Groggy>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Phase2Groggy");
		return nullptr;
	}

	return instance;
}
