#include "pch.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "StateEm3000_Transform.h"


StateEm3000_Transform::StateEm3000_Transform(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Transform::StateEnterInvoke()
{
	m_Owner.lock()->Tint_StageSpots(Em3000::RageSpotColor);
	Play(ANIM::PHASE_CHANGE_3, false);
	if (const auto client = ClientApp::GetInstance())
		client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_RAGE);
	return true;
}

void StateEm3000_Transform::Update(Float timeDelta)
{
	if (Is_Finished(ANIM::PHASE_CHANGE_3))
		Change_State(STATE::PUPPET);
}

void StateEm3000_Transform::StateExitInvoke()
{
}

Shared<StateEm3000_Transform> StateEm3000_Transform::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Transform>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Transform");
		return nullptr;
	}

	return instance;
}
