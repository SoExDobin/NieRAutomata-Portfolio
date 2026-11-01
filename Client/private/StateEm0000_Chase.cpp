#include "pch.h"
#include "StateEm0000_Chase.h"

#include "Em0000.h"
#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using msState = MonsterStateMachine::MONSTER_STATE;

StateEm0000_Chase::StateEm0000_Chase(const wstring& tag, const Shared<Em0000>& owner)
	: StateEm0000(tag, owner) {}

Bool StateEm0000_Chase::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;

	m_FarAttackCheck = n_FarAttackCheckInterval;
	m_Body.lock()->Set_Animation(ETOI(m_Owner.lock()->Get_ChaseClip()), 0.2f, true);
	return true;
}

void StateEm0000_Chase::Update(Float timeDelta)
{
	if (!Has_Target())
	{
		m_States.lock()->Change_State(msState::IDLE);
		return;
	}

	// The dash clip's root motion carries it forward while it turns toward the player.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = true;
	moveData.direction = Get_DirectionToTarget();
	m_Movement.lock()->Set_MovementData(moveData);

	const Float distance = Get_DistanceToTarget();
	const auto& reach = m_Owner.lock()->Get_AttackReach();
	if (distance <= reach.melee && Is_TargetFront())
	{
		m_States.lock()->Change_State(msState::ATTACK);
		return;
	}

	m_FarAttackCheck -= timeDelta;
	if (m_FarAttackCheck <= 0.f)
	{
		m_FarAttackCheck = n_FarAttackCheckInterval;
		if (distance >= reach.farMin && distance <= reach.farMax && Is_TargetFront() && Helper::Random_Bool(reach.farChance))
			m_States.lock()->Change_State(msState::ATTACK);
	}
}

Shared<StateEm0000_Chase> StateEm0000_Chase::Create(const wstring& tag, const Shared<Em0000>& owner)
{
	auto instance = make_shared<StateEm0000_Chase>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0000_Chase");
		return nullptr;
	}

	return instance;
}
