#include "pch.h"
#include "StateEm0010_Attack.h"

#include <SpdLogger.h>

#include "Em0010.h"
#include "Em0010Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using em0010Anim = Em0010::EM0010_STATE;
using msState = MonsterStateMachine::MONSTER_STATE;

StateEm0010_Attack::StateEm0010_Attack(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010{tag, owner} {}

HRESULT StateEm0010_Attack::Initialize()
{	
	m_EnterAnim.emplace(ETOI(em0010Anim::SPRINT_END));

	m_AttackAnim.emplace(ETOI(em0010Anim::PUNCH_FRONT));
	m_AttackAnim.emplace(ETOI(em0010Anim::SWING_FRONT));
	m_AttackAnim.emplace(ETOI(em0010Anim::FOOT_ATTACK));

	return StateEm0010::Initialize();
}

Bool StateEm0010_Attack::StateEnterInvoke()
{
	m_Movement.lock()->Reduce_RootMotion(); 

	auto em0010 = m_Body.lock();
	auto state = m_States.lock();

	// Ranged shoots unless the target is already within punching distance.
	m_IsShooting = Is_Ranged() && Get_DistanceToTarget() > n_RangedMeleeDistance;
	if (m_IsShooting)
	{
		m_ShotCount = 0;
		m_ShotTimer = 0.f;
		em0010->Set_Animation(ETOI(em0010Anim::SHOOT_START), 0.2f, false);
		return true;
	}

	switch (auto prevState = state->Get_CurMonsterState())
	{
	case msState::CHASE:
		em0010->Set_Animation(ETOI(em0010Anim::SPRINT_END), 0.2f, false);
		return true;
	case msState::AWAIT:
		Em0010Attack();
		return true;
	default:
		LOG_ERROR(L"[ENTER ATTACK] : No Enter state PrevIndex {} ", ETOI(prevState));
		return false;
	}
}

void StateEm0010_Attack::Update(Float timeDelta)
{
	if (m_IsShooting)
	{
		Update_Shoot(timeDelta);
		return;
	}

	auto em0010 = m_Body.lock();
	uint32 animIndex = em0010->Get_CurrentAnimationIndex();
	Bool isFinished = em0010->Is_AnimationFinished();

	if (m_EnterAnim.contains(animIndex) && isFinished)
	{
		Em0010Attack();
	}

	if (m_AttackAnim.contains(animIndex) && isFinished)
	{
		// Chain another swing half the time; otherwise back off into the strafe.
		Float dist = Get_DistanceToTarget();
		if (dist <= n_LeastDistance && Is_TargetFront() && Helper::Random_Bool(0.5))
		{
			Em0010Attack();
		}
		else
		{
			Change_ToEngageState();
		}
	}

}

void StateEm0010_Attack::Late_Update(Float timeDelta)
{
	
}

void StateEm0010_Attack::StateExitInvoke()
{
	m_Movement.lock()->Reset_RootMotionStop();
}

void StateEm0010_Attack::Update_Shoot(Float timeDelta)
{
	auto em0010 = m_Body.lock();
	uint32 animIndex = em0010->Get_CurrentAnimationIndex();
	Bool isFinished = em0010->Is_AnimationFinished();

	// Stand and keep turning toward the target while shooting.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = false;
	moveData.canRotation = true;
	moveData.lookDirection = Get_DirectionToTarget();
	m_Movement.lock()->Set_MovementData(moveData);

	if (animIndex == ETOI(em0010Anim::SHOOT_START))
	{
		if (isFinished)
			em0010->Set_Animation(ETOI(em0010Anim::SHOOT_LOOP), 0.1f, true);
		return;
	}

	if (animIndex == ETOI(em0010Anim::SHOOT_LOOP))
	{
		// The loop clip stays current while blending into SHOOT_END, so stop counting once the burst is out.
		if (m_ShotCount >= n_ShotsPerBurst) return;

		m_ShotTimer += timeDelta;
		if (m_ShotTimer < n_ShotInterval) return;

		m_ShotTimer -= n_ShotInterval;
		em0010->Fire_Bullet(Get_TargetAimPosition());
		if (++m_ShotCount >= n_ShotsPerBurst)
			em0010->Set_Animation(ETOI(em0010Anim::SHOOT_END), 0.2f, false);
		return;
	}

	if (animIndex == ETOI(em0010Anim::SHOOT_END) && isFinished)
		Change_ToEngageState();
}

void StateEm0010_Attack::Em0010Attack()
{
	auto em0010 = m_Body.lock();

	int32 rand = Helper::Random_Int(0, 2);
	uint32 anim[] = {
		ETOI(em0010Anim::PUNCH_FRONT),
		ETOI(em0010Anim::SWING_FRONT),
		ETOI(em0010Anim::FOOT_ATTACK),
	};

	em0010->Set_Animation(anim[rand], 0.2f, false);
}

Shared<StateEm0010_Attack> StateEm0010_Attack::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Attack>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0010_Attack");
		return nullptr;
	}

	return instance;
}
