#include "pch.h"
#include "StateEm0000_Idle.h"

#include "Em0000.h"
#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using em0000Anim = Em0000::EM0000_STATE;

StateEm0000_Idle::StateEm0000_Idle(const wstring& tag, const Shared<Em0000>& owner)
	: StateEm0000(tag, owner) {}

Bool StateEm0000_Idle::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;

	Hold_Position();
	// A statue (Emb001) holds its trumpet pose until enough damage wakes it.
	if (m_Owner.lock()->Is_Dormant())
		m_Body.lock()->Set_Animation(ETOI(em0000Anim::STATUE_POSE), 0.f, true);
	else
		m_Body.lock()->Set_Animation(ETOI(em0000Anim::IDLE), 0.2f, true);
	m_Visit = VISIT::REST;
	m_VisitTime = static_cast<Float>(Helper::Random_Double(1.0, 4.0));
	return true;
}

void StateEm0000_Idle::Update(Float timeDelta)
{
	if (m_Owner.lock()->Is_Dormant())
		return;

	// Woken: the fanfare clip lowers the trumpet and squares up to whoever broke the statue's sleep.
	const auto body = m_Body.lock();
	const uint32 animIndex = body->Get_CurrentAnimationIndex();
	if (animIndex == ETOI(em0000Anim::STATUE_POSE))
	{
		body->Set_Animation(ETOI(em0000Anim::WAKE), 0.2f, false);
		return;
	}
	if (animIndex == ETOI(em0000Anim::WAKE))
	{
		Hold_Position(Get_DirectionToTarget());
		if (!body->Is_AnimationFinished())
			return;
		if (!Has_Target())
		{
			Hold_Position();
			body->Set_Animation(ETOI(em0000Anim::IDLE), 0.2f, true);
			return;
		}
	}

	if (Has_Target())
	{
		m_States.lock()->Change_State(MonsterStateMachine::MONSTER_STATE::CHASE);
		return;
	}
	if (m_Owner.lock()->Is_Passive())
		Update_Visit(timeDelta);
}

// Rest a few seconds, walk to a point 3~8 m from where it was spawned, throw paper there, rest again.
void StateEm0000_Idle::Update_Visit(Float timeDelta)
{
	const auto body = m_Body.lock();
	switch (m_Visit)
	{
	case VISIT::REST:
		m_VisitTime -= timeDelta;
		if (m_VisitTime > 0.f)
			return;
		{
			const Float angle = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
			const Float reach = static_cast<Float>(Helper::Random_Double(3.0, 8.0));
			m_VisitPoint = m_Movement.lock()->Get_RootPosition() + Vector3{ cosf(angle) * reach, 0.f, sinf(angle) * reach };
		}
		m_Visit = VISIT::WALK;
		m_VisitTime = 0.f;
		body->Set_Animation(ETOI(em0000Anim::WALK_F), 0.2f, true);
		return;

	case VISIT::WALK:
	{
		m_VisitTime += timeDelta;
		Vector3 toPoint = m_VisitPoint - m_Owner.lock()->Get_Transform()->Get_Position();
		toPoint.y = 0.f;
		// A point it cannot reach (a wall, a fence) is given up after a while.
		if (toPoint.Length() > 0.8f && m_VisitTime < 8.f)
		{
			toPoint.Normalize();
			Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
			moveData.direction = toPoint;
			moveData.isMove = true;
			moveData.canRotation = true;
			moveData.useRootMotionDir = false;
			moveData.rootMotionScale = 1.f;
			m_Movement.lock()->Set_MovementData(moveData);
			return;
		}
		Hold_Position();
		m_Visit = VISIT::THROW;
		m_VisitTime = 0.f;
		body->Set_Animation(ETOI(em0000Anim::PAPER_THROW), 0.2f, false);
		return;
	}

	case VISIT::THROW:
		// Is_AnimationFinished still answers for the walk clip while the blend runs.
		m_VisitTime += timeDelta;
		if (m_VisitTime < 0.5f || !body->Is_AnimationFinished())
			return;
		body->Set_Animation(ETOI(em0000Anim::IDLE), 0.2f, true);
		m_Visit = VISIT::REST;
		m_VisitTime = static_cast<Float>(Helper::Random_Double(2.0, 5.0));
		return;
	}
}

Shared<StateEm0000_Idle> StateEm0000_Idle::Create(const wstring& tag, const Shared<Em0000>& owner)
{
	auto instance = make_shared<StateEm0000_Idle>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0000_Idle");
		return nullptr;
	}

	return instance;
}
