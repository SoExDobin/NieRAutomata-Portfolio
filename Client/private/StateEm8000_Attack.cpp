#include "pch.h"
#include "StateEm8000_Attack.h"

#include "Em8000Body.h"
#include "Random_Helper.h"

StateEm8000_Attack::StateEm8000_Attack(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Attack::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	switch (m_Owner.lock()->Get_NextAttack())
	{
	case ATTACK::SCISSORS:		m_Clip = ANIM::SCISSORS; m_SteerTime = BladeSteerTime; break;
	case ATTACK::BLADE_SLAM:	m_Clip = ANIM::BLADE_SLAM; m_SteerTime = BladeSteerTime; break;
	case ATTACK::JUMP_SCISSORS:	m_Clip = ANIM::JUMP_SCISSORS; m_SteerTime = JumpSteerTime; break;
	case ATTACK::JUMP_PRESS:	m_Clip = ANIM::JUMP_PRESS; m_SteerTime = StompSteerTime; break;
	case ATTACK::TAIL_SWEEP:	m_Clip = ANIM::TAIL_SWEEP; m_SteerTime = TailSteerTime; break;
	case ATTACK::TAIL_SLAM:		m_Clip = ANIM::TAIL_SLAM; m_SteerTime = SlamSteerTime; break;
	default:					m_Clip = Helper::Random_Bool(0.5) ? ANIM::STOMP_A : ANIM::STOMP_B; m_SteerTime = StompSteerTime; break;
	}
	m_Elapsed = 0.f;
	Play(m_Clip, false);
	m_Body.lock()->Telegraph();
	return true;
}

void StateEm8000_Attack::Update(Float timeDelta)
{
	m_Elapsed += timeDelta;
	Follow_RootMotion(m_Elapsed < m_SteerTime);
	if (Is_Finished(m_Clip))
		Change_State(STATE::IDLE);
}

void StateEm8000_Attack::StateExitInvoke()
{
	m_Body.lock()->End_Attacks();
}

Shared<StateEm8000_Attack> StateEm8000_Attack::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Attack>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Attack");
		return nullptr;
	}

	return instance;
}
