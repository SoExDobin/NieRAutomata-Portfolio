#include "pch.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "StateEm3000_Dead.h"

#include <Game.h>

#include "Em3000.h"
#include "Em3000Body.h"
#include "Em3000Parts.h"
#include "EmMovement.h"
#include "Pl0000.h"

StateEm3000_Dead::StateEm3000_Dead(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Dead::StateEnterInvoke()
{
	m_Elapsed = 0.f;
	m_Finished = false;
	m_Body.lock()->End_Attacks();
	m_Movement.lock()->Set_MovementData({});
	Play(ANIM::SCREAMING, false, 0.2f);
	// Her last words start as she goes down, before the blast; the Pod's kill confirmation follows them.
	if (const auto client = ClientApp::GetInstance())
		client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::AFTER_EM3000);
	return true;
}

void StateEm3000_Dead::Update(Float timeDelta)
{
	if (m_Finished) return;

	const Float previous = m_Elapsed;
	m_Elapsed += timeDelta;
	if (previous < CryTime && m_Elapsed >= CryTime)
		m_Body.lock()->Play_Voice(L"em3000_vo_cry");
	if (m_Elapsed < BlastTime) return;

	const Shared<Em3000Body> body = m_Body.lock();
	body->Burst(FinalBlastSize, true);
	body->Play_Voice(L"em3000_vo_death");
	GAME_INSTANCE->StopSound(Em3000::BgmChannel);
	GAME_INSTANCE->FadeSound(Pl0000::LevelBgmChannel, 1.f, Pl0000::LevelBgmFadeInTime);
	// The pieces fly out from 1 m above the boss's feet and land on the floor it stands on.
	const Vector3 center = m_Owner.lock()->Get_Transform()->Get_Position() + Vector3{ 0.f, 1.f, 0.f };
	body->Begin_Shatter(center);
	m_Movement.lock()->Set_Active(false);
	if (const auto chains = Cast<Em3000Parts>(m_Owner.lock()->Find_Child(L"Em3001")); chains && chains->Is_Active())
		chains->Begin_Shatter(center);
	m_Finished = true;
}

void StateEm3000_Dead::StateExitInvoke()
{
}

Shared<StateEm3000_Dead> StateEm3000_Dead::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Dead>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Dead");
		return nullptr;
	}

	return instance;
}
