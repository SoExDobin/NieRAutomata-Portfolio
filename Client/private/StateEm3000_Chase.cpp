#include "pch.h"
#include "StateEm3000_Chase.h"

#include "Em3000Body.h"
#include "Random_Helper.h"


StateEm3000_Chase::StateEm3000_Chase(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Chase::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_VoiceWait = 0.f;
	// Back from a stomp it chases a moment before it may stomp again.
	m_StompWait = StompRetryTime;
	Play(ANIM::READY_CHASE, false);
	return true;
}

void StateEm3000_Chase::Update(Float timeDelta)
{
	m_VoiceWait -= timeDelta;
	if (m_VoiceWait <= 0.f)
	{
		m_VoiceWait = static_cast<Float>(Helper::Random_Double(VoiceMinWait, VoiceMaxWait));
		m_Body.lock()->Play_Voice(L"em3000_vo_chase");
	}

	if (Is_Finished(ANIM::READY_CHASE))
		Play(ANIM::CHASE, true);
	if (!m_Owner.lock()->Has_LivingPuppets())
	{
		Change_State(STATE::PHASE2_GROGGY);
		return;
	}

	m_StompWait -= timeDelta;
	if (m_StompWait <= 0.f && Has_Target() && Get_DistanceToTarget() <= StompRange)
	{
		m_StompWait = StompRetryTime;
		if (Helper::Random_Double(0.0, 1.0) < StompChance)
		{
			Change_State(STATE::STOMP);
			return;
		}
	}
	Follow_RootMotion(true);
}

void StateEm3000_Chase::StateExitInvoke()
{
}

Shared<StateEm3000_Chase> StateEm3000_Chase::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Chase>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Chase");
		return nullptr;
	}

	return instance;
}
