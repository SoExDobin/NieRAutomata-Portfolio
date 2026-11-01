#include "pch.h"
#include "StateEm8000_Groggy.h"

#include "Em8000Body.h"

StateEm8000_Groggy::StateEm8000_Groggy(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Groggy::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_Body.lock()->End_Attacks();
	m_LoopTime = 0.f;
	Play(ANIM::GROGGY_IN, false);
	return true;
}

void StateEm8000_Groggy::Update(Float timeDelta)
{
	Hold_Still();
	if (Is_Finished(ANIM::GROGGY_IN))
	{
		Play(ANIM::GROGGY_LOOP, true, 0.1f);
		m_Body.lock()->Play_LoopSound(L"em8000_dizzy_loop");
		return;
	}
	if (Is_Playing(ANIM::GROGGY_LOOP))
	{
		m_LoopTime += timeDelta;
		if (m_LoopTime >= m_Owner.lock()->Get_GroggyTime())
		{
			m_Body.lock()->Stop_LoopSound();
			Play(ANIM::GROGGY_END, false, 0.1f);
		}
		return;
	}
	if (Is_Finished(ANIM::GROGGY_END))
		Change_State(STATE::IDLE);
}

void StateEm8000_Groggy::StateExitInvoke()
{
	m_Body.lock()->Stop_LoopSound();
}

Shared<StateEm8000_Groggy> StateEm8000_Groggy::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Groggy>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Groggy");
		return nullptr;
	}

	return instance;
}
