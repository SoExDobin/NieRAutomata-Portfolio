#include "pch.h"
#include "StateEm3000_Stomp.h"


StateEm3000_Stomp::StateEm3000_Stomp(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Stomp::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	Play(ANIM::JUMP_STOMP, false);
	return true;
}

void StateEm3000_Stomp::Update(Float timeDelta)
{
	Follow_RootMotion(true);
	if (!Is_Finished(ANIM::JUMP_STOMP)) return;

	// Chase decides whether to stomp again (StateEm3000_Chase::StompChance).
	if (!m_Owner.lock()->Has_LivingPuppets())
		Change_State(STATE::PHASE2_GROGGY);
	else
		Change_State(STATE::CHASE);
}

void StateEm3000_Stomp::StateExitInvoke()
{
}

Shared<StateEm3000_Stomp> StateEm3000_Stomp::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Stomp>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Stomp");
		return nullptr;
	}

	return instance;
}
