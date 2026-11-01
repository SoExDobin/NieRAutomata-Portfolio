#include "pch.h"
#include "StateEm3000_Groggy.h"
#include "ClientApp.h"
#include "UIManager.h"

#include "Em3000Body.h"
#include "EmMovement.h"

StateEm3000_Groggy::StateEm3000_Groggy(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Groggy::StateEnterInvoke()
{
	m_Body.lock()->End_Attacks();
	m_Movement.lock()->Set_MovementData({});
	m_LoopTime = 0.f;
	Play(ANIM::GROGGY_IN, false);
	if (const auto client = ClientApp::GetInstance())
		client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_GROGGY);
	return true;
}

void StateEm3000_Groggy::Update(Float timeDelta)
{
	if (Is_Finished(ANIM::GROGGY_IN))
	{
		Play(ANIM::GROGGY_LOOP, true);
		m_Body.lock()->Play_LoopSound(L"em3000_dizzy_loop");
		return;
	}
	if (Is_Playing(ANIM::GROGGY_LOOP))
	{
		m_LoopTime += timeDelta;
		if (m_LoopTime >= LoopDuration)
		{
			m_Body.lock()->Stop_LoopSound();
			Play(ANIM::GROGGY_END, false);
		}
		return;
	}
	if (!Is_Finished(ANIM::GROGGY_END)) return;

	// The dress blows off with a boom; the transform (100a) shouts and howls over it.
	const Shared<Em3000> owner = m_Owner.lock();
	m_Body.lock()->Burst(BlastSize, true);
	// The head cloth and the skirt burn away where they hang; the head chains stay until the boss dies.
	for (const wstring& part : { L"Em3002", L"Em3003" })
		if (const auto cloth = Cast<Em3000Parts>(owner->Find_Child(part)))
			cloth->Begin_Burn();

	Change_State(STATE::TRANSFORM);
}

void StateEm3000_Groggy::StateExitInvoke()
{
	m_Body.lock()->Stop_LoopSound();
}

Shared<StateEm3000_Groggy> StateEm3000_Groggy::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Groggy>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Groggy");
		return nullptr;
	}

	return instance;
}
