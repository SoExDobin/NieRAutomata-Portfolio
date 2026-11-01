#include "pch.h"
#include "StateEm0000_Attack.h"

#include "Em0000.h"
#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using em0000Anim = Em0000::EM0000_STATE;

StateEm0000_Attack::StateEm0000_Attack(const wstring& tag, const Shared<Em0000>& owner)
	: StateEm0000(tag, owner) {}

Bool StateEm0000_Attack::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead() || !Has_Target()) return false;

	m_LoopTime = 0.f;
	m_Body.lock()->Set_Animation(ETOI(m_Owner.lock()->Choose_Attack(Get_DistanceToTarget())), 0.2f, false);
	return true;
}

void StateEm0000_Attack::Update(Float timeDelta)
{
	auto body = m_Body.lock();
	const uint32 animIndex = body->Get_CurrentAnimationIndex();
	const Bool blowing = animIndex == ETOI(em0000Anim::STATUE_POSE);
	const Bool aiming = blowing || (animIndex != ETOI(em0000Anim::TACKLE_SLIDE) &&
		animIndex != ETOI(em0000Anim::TACKLE_GETUP) && animIndex != ETOI(em0000Anim::TRUMPET_END) &&
		body->Get_AnimationProgress() < n_AimUntil);

	// The clips' root motion carries every attack; it keeps turning only while winding up, and while it blows the trumpet.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isAttack = true;
	moveData.canRotation = aiming && Has_Target();
	moveData.lookDirection = moveData.canRotation ? Get_DirectionToTarget() : Vector3::Zero;
	m_Movement.lock()->Set_MovementData(moveData);

	if (blowing)
	{
		m_LoopTime += timeDelta;
		if (m_LoopTime >= n_TrumpetTime)
			body->Set_Animation(ETOI(em0000Anim::TRUMPET_END), 0.1f, false);
		return;
	}

	if (!body->Is_AnimationFinished())
		return;

	if (animIndex == ETOI(em0000Anim::TACKLE_START))
		body->Set_Animation(ETOI(em0000Anim::TACKLE_SLIDE), 0.05f, false);
	else if (animIndex == ETOI(em0000Anim::TACKLE_SLIDE))
		body->Set_Animation(ETOI(em0000Anim::TACKLE_GETUP), 0.05f, false);
	else if (animIndex == ETOI(em0000Anim::TRUMPET_START))
		body->Set_Animation(ETOI(em0000Anim::STATUE_POSE), 0.05f, true);
	else
		Change_ToEngageState();
}

void StateEm0000_Attack::StateExitInvoke()
{
	// A landed blow slows the root motion (OnAttackHit); the next move starts at full speed again.
	m_Movement.lock()->Reset_RootMotionStop();
}

Shared<StateEm0000_Attack> StateEm0000_Attack::Create(const wstring& tag, const Shared<Em0000>& owner)
{
	auto instance = make_shared<StateEm0000_Attack>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0000_Attack");
		return nullptr;
	}

	return instance;
}
