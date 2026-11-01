#include "pch.h"
#include "StateEm3000_Puppet.h"

#include "Em3000Body.h"


StateEm3000_Puppet::StateEm3000_Puppet(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Puppet::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_Summoned = false;
	m_Elapsed = 0.f;
	m_Owner.lock()->Tint_StageSpots(Em3000::RageSpotColor);
	Hold_FacingTarget();
	Play(ANIM::SCREAMING, false);
	return true;
}

void StateEm3000_Puppet::Update(Float timeDelta)
{
	m_Elapsed += timeDelta;
	if (!m_Summoned && m_Elapsed >= SummonTime)
	{
		m_Summoned = true;
		m_Owner.lock()->Summon_Puppets();
	}
	if (Is_Finished(ANIM::SCREAMING))
		Change_State(STATE::CHASE);
}

void StateEm3000_Puppet::StateExitInvoke()
{
}

Shared<StateEm3000_Puppet> StateEm3000_Puppet::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Puppet>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Puppet");
		return nullptr;
	}

	return instance;
}
