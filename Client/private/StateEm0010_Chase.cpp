#include "pch.h"
#include "StateEm0010_Chase.h"

#include "Em0010.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using em0010Anim = Em0010::EM0010_STATE;
using msState = MonsterStateMachine::MONSTER_STATE;

StateEm0010_Chase::StateEm0010_Chase(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010{ tag, owner } {}

HRESULT StateEm0010_Chase::Initialize()
{
	m_EnterAnim.emplace(ETOI(em0010Anim::START_SPRINT_1));
	m_EnterAnim.emplace(ETOI(em0010Anim::START_SPRINT_2));
	m_EnterAnim.emplace(ETOI(em0010Anim::START_SPRINT_3));

	return StateEm0010::Initialize();
}

Bool StateEm0010_Chase::StateEnterInvoke()
{
	m_CanAwait = m_States.lock()->Get_CurMonsterState() != msState::AWAIT;

	uint32 rand = Helper::Random_Int(ETOI(em0010Anim::START_SPRINT_1), ETOI(em0010Anim::START_SPRINT_3));

	m_Body.lock()->Set_Animation(rand, 0.2f, false);

	return true;
}

void StateEm0010_Chase::Update(Float timeDelta)
{
	auto em0010 = m_Body.lock();
	auto movement = m_Movement.lock();
	Bool isFinished = em0010->Is_AnimationFinished();
	uint32 animIndex = em0010->Get_CurrentAnimationIndex();

	Em0010Movement::EM0010_MOVEMENT_DATA movementData{};
	movementData.isMove = true;
	movementData.direction = Get_DirectionToTarget();

	movement->Set_MovementData(movementData);

	if (m_EnterAnim.contains(animIndex) && isFinished)
	{
		int32 rand = Helper::Random_Int(ETOI(em0010Anim::SPRINT_1), ETOI(em0010Anim::SPRINT_2));
		m_Body.lock()->Set_Animation(rand, 0.2f, true);
	}


	Float dist = Get_DistanceToTarget();
	const Bool startAwait = Is_Ranged() ? dist <= n_RangedEngageDistance
		: m_CanAwait && dist <= Get_KeepDistanceMax();
	if (startAwait)
	{
		m_States.lock()->Change_State(msState::AWAIT);
		return;
	}

	if (dist <= n_LeastDistance && Is_TargetFront())
	{
		m_States.lock()->Change_State(msState::ATTACK);
	}

}

void StateEm0010_Chase::Late_Update(Float timeDelta)
{
	
}

void StateEm0010_Chase::StateExitInvoke()
{
	
}


Shared<StateEm0010_Chase> StateEm0010_Chase::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Chase>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0010_Chase");
		return nullptr;
	}

	return instance;
}

