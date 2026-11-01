#include "pch.h"
#include "StateEm0010_Await.h"

#include "Em0010.h"
#include "Em0010Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

using em0010Anim = Em0010::EM0010_STATE;
using msState = MonsterStateMachine::MONSTER_STATE;

StateEm0010_Await::StateEm0010_Await(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010{ tag, owner } {}

HRESULT StateEm0010_Await::Initialize()
{
	return StateEm0010::Initialize();
}

Bool StateEm0010_Await::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	if (false == Has_Target()) return false;

	m_HasFlipped = false;
	m_BackStepCooldown = 0.f;
	Start_Strafe(Helper::Random_Bool(0.5));

	return true;
}

void StateEm0010_Await::Update(Float timeDelta)
{
	if (false == Has_Target())
	{
		m_States.lock()->Change_State(msState::IDLE);
		return;
	}

	// Root motion of the side-step clips moves sideways in local space while the body keeps facing the target.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = true;
	moveData.canRotation = true;
	moveData.useRootMotionDir = true;
	moveData.lookDirection = Get_DirectionToTarget();
	moveData.rootMotionScale = 1.f;
	m_Movement.lock()->Set_MovementData(moveData);

	m_BackStepCooldown -= timeDelta;

	if (m_IsBackStepping)
	{
		if (m_Body.lock()->Is_AnimationFinished())
			Start_Strafe(m_StrafeRight);
		return;
	}

	const Float dist = Get_DistanceToTarget();
	if (dist > Get_KeepDistanceMax())
	{
		m_States.lock()->Change_State(msState::CHASE);
		return;
	}

	m_StrafeElapsed += timeDelta;

	if (dist < Get_KeepDistanceMin())
	{
		if (Is_Ranged())
		{
			if (dist <= n_RangedMeleeDistance && Is_TargetFront())
			{
				m_States.lock()->Change_State(msState::ATTACK);
				return;
			}
			if (m_BackStepCooldown <= 0.f)
			{
				Start_BackStep();
				return;
			}
		}
		else if (m_StrafeElapsed >= n_MeleeStrikeDelay && Is_TargetFront())
		{
			m_States.lock()->Change_State(msState::ATTACK);
			return;
		}
	}

	Update_Stuck(timeDelta);

	if (m_StrafeElapsed >= m_StrafeDuration)
		On_StrafeTimeout();
}

void StateEm0010_Await::Late_Update(Float timeDelta)
{

}

void StateEm0010_Await::StateExitInvoke()
{
	if (auto movement = m_Movement.lock())
	{
		Em0010Movement::EM0010_MOVEMENT_DATA stopData{};
		stopData.isMove = false;
		stopData.canRotation = false;
		movement->Set_MovementData(stopData);
	}
}

void StateEm0010_Await::Start_Strafe(Bool toRight)
{
	m_StrafeRight = toRight;
	m_IsBackStepping = false;
	m_StrafeDuration = static_cast<Float>(Helper::Random_Double(1.2, 2.5));
	m_StrafeElapsed = 0.f;
	m_StuckElapsed = 0.f;
	m_LastPosition = m_Owner.lock()->Get_Transform()->Get_Position();

	const em0010Anim anim = toRight ? em0010Anim::WALK_LOOK_TARGET_R : em0010Anim::WALK_LOOK_TARGET_L;
	m_Body.lock()->Set_Animation(ETOI(anim), 0.2f, true);
}

void StateEm0010_Await::Start_BackStep()
{
	m_IsBackStepping = true;
	m_BackStepCooldown = n_BackStepInterval;
	m_Body.lock()->Set_Animation(ETOI(em0010Anim::ESCAPE_BACK), 0.2f, false);
}

void StateEm0010_Await::Update_Stuck(Float timeDelta)
{
	m_StuckElapsed += timeDelta;
	if (m_StuckElapsed < n_StuckTime)
		return;

	// Blocked by a wall or another monster: step the other way.
	const Vector3 position = m_Owner.lock()->Get_Transform()->Get_Position();
	if (Vector3::Distance(position, m_LastPosition) < n_StuckDistance)
	{
		m_HasFlipped = true;
		Start_Strafe(!m_StrafeRight);
		return;
	}

	m_StuckElapsed = 0.f;
	m_LastPosition = position;
}

void StateEm0010_Await::On_StrafeTimeout()
{
	if (false == m_HasFlipped && Helper::Random_Bool(0.5))
	{
		m_HasFlipped = true;
		Start_Strafe(!m_StrafeRight);
		return;
	}

	// Ranged shoots from here; melee runs in and strikes (Chase does not return to AWAIT right after it).
	if (Is_Ranged())
		m_States.lock()->Change_State(msState::ATTACK);
	else
		m_States.lock()->Change_State(msState::CHASE);
}

Shared<StateEm0010_Await> StateEm0010_Await::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Await>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0010_Await");
		return nullptr;
	}

	return instance;
}
