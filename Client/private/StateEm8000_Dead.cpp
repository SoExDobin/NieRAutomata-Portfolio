#include "pch.h"
#include "StateEm8000_Dead.h"

#include "Em8000Body.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "Random_Helper.h"

StateEm8000_Dead::StateEm8000_Dead(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Dead::StateEnterInvoke()
{
	m_Elapsed = 0.f;
	m_NextBlast = 0.f;
	m_Finished = false;
	m_Body.lock()->End_Attacks();
	m_Body.lock()->Power_Down();
	Hold_Still();
	// The ball unfolds back onto its legs (0541) as it breaks up, then collapses like the walker (0190).
	m_Unfolding = m_Owner.lock()->Is_BallForm();
	if (m_Unfolding)
	{
		m_Body.lock()->Hide_Armour();
		Play(ANIM::BALL_OUT, false, 0.3f);
	}
	else
		Play(ANIM::DEAD, false);
	return true;
}

void StateEm8000_Dead::Update(Float timeDelta)
{
	Hold_Still();
	if (m_Finished) return;

	const Shared<Em8000Body> body = m_Body.lock();
	m_Elapsed += timeDelta;
	// The blasts already go off while it unfolds; the chain up to the big blast runs from the collapse.
	if (m_Unfolding && Is_Finished(ANIM::BALL_OUT))
	{
		m_Unfolding = false;
		m_Elapsed = 0.f;
		m_NextBlast = 0.f;
		Play(ANIM::DEAD, false);
	}
	if (!m_Unfolding && m_Elapsed >= ChainTime)
	{
		body->Burst(FinalBlastSize, true);
		m_Finished = true;
        if (const auto client = ClientApp::GetInstance())
            client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::AFTER_EM8000);
		return;
	}
	if (m_Elapsed >= m_NextBlast)
	{
		m_NextBlast += BlastInterval;
		body->Burst(static_cast<Float>(Helper::Random_Double(3., 7.)), false);
	}
}

void StateEm8000_Dead::StateExitInvoke()
{
}

Shared<StateEm8000_Dead> StateEm8000_Dead::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Dead>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Dead");
		return nullptr;
	}

	return instance;
}
