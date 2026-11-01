#include "pch.h"
#include "StateEm8000_Gun.h"

#include "Em8000Body.h"

StateEm8000_Gun::StateEm8000_Gun(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Gun::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_Shotgun = m_Owner.lock()->Get_NextAttack() != ATTACK::SPIN_FIRE;
	m_Start = m_Shotgun ? ANIM::SHOTGUN_START : ANIM::SPIN_FIRE_START;
	m_Loop = m_Shotgun ? ANIM::SHOTGUN_LOOP : ANIM::SPIN_FIRE_LOOP;
	m_End = m_Shotgun ? ANIM::SHOTGUN_END : ANIM::SPIN_FIRE_END;
	m_Time = 0.f;
	m_NextShot = m_Shotgun ? ShotgunFirst : 0.f;
	Play(m_Start, false);
	m_Body.lock()->Telegraph();
	return true;
}

void StateEm8000_Gun::Update(Float timeDelta)
{
	if (Is_Playing(m_Start))
	{
		Hold_FacingTarget();
		if (!Is_Finished(m_Start)) return;
		Turn_ToBody();
		Play(m_Loop, true, 0.f);
		return;
	}
	if (Is_Playing(m_Loop))
	{
		// The shotgun keeps turning to the player; the spin is the clip's own.
		if (m_Shotgun)
			Hold_FacingTarget();
		else
			Hold_Still();
		m_Time += timeDelta;
		while (m_Time >= m_NextShot)
		{
			if (m_Shotgun)
				m_Body.lock()->Fire_Gun(ShotgunPellets, ShotgunSpread);
			else
				m_Body.lock()->Fire_Gun(SpinPellets, SpinSpread);
			m_NextShot += m_Shotgun ? ShotgunInterval : SpinInterval;
		}
		if (m_Time >= (m_Shotgun ? ShotgunTime : SpinTime))
			Play(m_End, false, 0.f);
		return;
	}
	Hold_Still();
	if (Is_Finished(m_End))
	{
		Turn_ToBody();
		Play(ANIM::IDLE, true, 0.f);
		Change_State(STATE::IDLE);
	}
}

void StateEm8000_Gun::StateExitInvoke()
{
	m_Body.lock()->End_Attacks();
}

Shared<StateEm8000_Gun> StateEm8000_Gun::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Gun>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Gun");
		return nullptr;
	}

	return instance;
}
