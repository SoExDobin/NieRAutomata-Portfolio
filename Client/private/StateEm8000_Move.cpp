#include "pch.h"
#include "StateEm8000_Move.h"

#include "Em8000Body.h"
#include "EmMovement.h"
#include "Random_Helper.h"

StateEm8000_Move::StateEm8000_Move(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Move::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	const Float distance = Get_DistanceToTarget();
	if (distance < BackOffRange)
		m_Start = ANIM::MOVE_B_START;
	else if (distance > CloseInRange)
		m_Start = ANIM::MOVE_F_START;
	else
		m_Start = Helper::Random_Bool(0.5) ? ANIM::MOVE_L_START : ANIM::MOVE_R_START;
	m_LoopTime = 0.f;
	m_LoopDuration = static_cast<Float>(Helper::Random_Double(MinLoop, MaxLoop));
	Play(m_Start, false);
	return true;
}

void StateEm8000_Move::Update(Float timeDelta)
{
	// The clips carry the travel in their own frame; the body keeps turning to the player.
	EmMovement::EM_MOVEMENT_DATA data{};
	data.isMove = true;
	data.lookDirection = Get_DirectionToTarget();
	m_Movement.lock()->Set_MovementData(data);

	const ANIM loop = static_cast<ANIM>(ETOI(m_Start) + 1);
	const ANIM end = static_cast<ANIM>(ETOI(m_Start) + 2);
	if (Is_Finished(m_Start))
	{
		Play(loop, true, 0.1f);
		m_Body.lock()->Play_LoopSound(L"em8000_hover_loop");
		return;
	}
	if (Is_Playing(loop))
	{
		m_LoopTime += timeDelta;
		const Bool closeEnough = m_Start == ANIM::MOVE_F_START && Get_DistanceToTarget() < CloseInStop;
		if (m_LoopTime >= m_LoopDuration || closeEnough)
		{
			m_Body.lock()->Stop_LoopSound();
			Play(end, false, 0.1f);
		}
		return;
	}
	if (Is_Finished(end))
		Change_State(STATE::IDLE);
}

void StateEm8000_Move::StateExitInvoke()
{
	m_Body.lock()->Stop_LoopSound();
}

Shared<StateEm8000_Move> StateEm8000_Move::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Move>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Move");
		return nullptr;
	}

	return instance;
}
