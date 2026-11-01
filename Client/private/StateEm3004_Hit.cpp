#include "pch.h"
#include "StateEm3004_Hit.h"

StateEm3004_Hit::StateEm3004_Hit(const wstring& tag, const Shared<Em3004>& owner)
	: StateEm3004{ tag, owner } {}

Bool StateEm3004_Hit::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	Hold_Still();
	Play(ANIM::FLINCH, false);
	return true;
}

void StateEm3004_Hit::Update(Float timeDelta)
{
	if (Is_Finished(ANIM::FLINCH))
		Change_State(STATE::ATTACK);
}

Shared<StateEm3004_Hit> StateEm3004_Hit::Create(const wstring& tag, const Shared<Em3004>& owner)
{
	auto instance = make_shared<StateEm3004_Hit>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3004_Hit");
		return nullptr;
	}

	return instance;
}
