#include "pch.h"
#include "StateEm3000_Idle.h"

#include "Random_Helper.h"
#include "StateEm3000_Range.h"

StateEm3000_Idle::StateEm3000_Idle(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Idle::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	Play(ANIM::IDLE, true, 0.2f);
	m_Delay = static_cast<Float>(Helper::Random_Double(0.5, 2.0));
	return true;
}

void StateEm3000_Idle::Update(Float timeDelta)
{
	Hold_FacingTarget();
	m_Delay -= timeDelta;
	if (m_Delay > 0.f || !Has_Target()) return;

	// The first round shows every pattern once in order wherever the player stands: each ranged pattern (Range takes
	// them in order on its first visits), then the blade mixer.
	if (m_Visits <= StateEm3000_Range::PatternCount)
	{
		Change_State(m_Visits++ < StateEm3000_Range::PatternCount ? STATE::RANGE : STATE::MELEE);
		return;
	}
	// Far away it is mostly a ranged pattern, but sometimes the blade mixer, whose circle sweeps out to the player.
	if (Get_DistanceToTarget() <= MeleeRange || Helper::Random_Double(0.0, 1.0) < FarMixerChance)
		Change_State(STATE::MELEE);
	else
		Change_State(STATE::RANGE);
}

void StateEm3000_Idle::StateExitInvoke()
{
}

Shared<StateEm3000_Idle> StateEm3000_Idle::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Idle>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Idle");
		return nullptr;
	}

	return instance;
}
