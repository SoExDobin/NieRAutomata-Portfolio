#include "pch.h"
#include "StateEm0000_Await.h"

#include "Em0000.h"
#include "Em0000Body.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using msState = MonsterStateMachine::MONSTER_STATE;

StateEm0000_Await::StateEm0000_Await(const wstring& tag, const Shared<Em0000>& owner)
	: StateEm0000(tag, owner) {}

Bool StateEm0000_Await::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead() || !Has_Target()) return false;

	m_Remaining = static_cast<Float>(Helper::Random_Double(0.5, 1.2));
	m_Body.lock()->Set_Animation(ETOI(Em0000::EM0000_STATE::IDLE), 0.2f, true);
	return true;
}

void StateEm0000_Await::Update(Float timeDelta)
{
	if (!Has_Target())
	{
		m_States.lock()->Change_State(msState::IDLE);
		return;
	}

	Hold_Position(Get_DirectionToTarget());

	m_Remaining -= timeDelta;
	if (m_Remaining > 0.f)
		return;

	// Strikes at once in melee reach, and rolls the far attack in its range (Emb001 steps back to blow the trumpet).
	const Float distance = Get_DistanceToTarget();
	const auto reach = m_Owner.lock()->Get_AttackReach();
	const Bool inMelee = distance <= reach.melee;
	const Bool inFarRange = distance >= reach.farMin && distance <= reach.farMax && Helper::Random_Bool(reach.farChance);
	if ((inMelee || inFarRange) && Is_TargetFront())
		m_States.lock()->Change_State(msState::ATTACK);
	else
		m_States.lock()->Change_State(msState::CHASE);
}

Shared<StateEm0000_Await> StateEm0000_Await::Create(const wstring& tag, const Shared<Em0000>& owner)
{
	auto instance = make_shared<StateEm0000_Await>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0000_Await");
		return nullptr;
	}

	return instance;
}
