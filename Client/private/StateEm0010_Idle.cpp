#include "pch.h"
#include "StateEm0010_Idle.h"

#include <SpdLogger.h>

#include "Em0010.h"
#include "Em0010Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

StateEm0010_Idle::StateEm0010_Idle(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010(tag, owner) {}

HRESULT StateEm0010_Idle::Initialize()
{
	m_EnterAnim.emplace(ETOI(Em0010::EM0010_STATE::WALK_END));

	if (!m_Owner.expired())
		m_RootPosition = m_Owner.lock()->Get_Transform()->Get_Position();

	return StateEm0010::Initialize();
}


Bool StateEm0010_Idle::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	auto em0010 = m_Body.lock();
	m_Saluting = false;

	switch (auto prevState = m_States.lock()->Get_CurMonsterState())
	{
	case MonsterStateMachine::MONSTER_STATE::Hit:
	case MonsterStateMachine::MONSTER_STATE::AWAIT:
		em0010->Set_Animation(ETOI(Em0010::EM0010_STATE::IDLE), 0.2f, false);
		return true;
	case MonsterStateMachine::MONSTER_STATE::WALK:
		em0010->Set_Animation(ETOI(Em0010::EM0010_STATE::WALK_END), 0.2f, false);
		return true;
	case MonsterStateMachine::MONSTER_STATE::CHASE:
		em0010->Set_Animation(ETOI(Em0010::EM0010_STATE::SPRINT_END), 0.2f, true);
		return true;
	default:
	{
		if (m_InitializeState)
		{
			em0010->Set_Animation(ETOI(Em0010::EM0010_STATE::IDLE), 0.2f, true);
			m_InitializeState = false;

			return true;
		}
		LOG_ERROR(L"[ENTER IDLE] : No Enter state PrevIndex {} ", ETOI(prevState));
		return false;
	}
	}
}

void StateEm0010_Idle::Update(Float timeDelta)
{
	auto em0010Body = m_Body.lock();
	auto animIndex = em0010Body->Get_CurrentAnimationIndex();
	Bool isFinished = em0010Body->Is_AnimationFinished();

	if (m_EnterAnim.contains(animIndex) && isFinished)
	{
		em0010Body->Set_Animation(ETOI(Em0010::EM0010_STATE::IDLE), 0.2f, true);
	}

	if (Has_Target())
	{
		m_States.lock()->Change_State(MonsterStateMachine::MONSTER_STATE::CHASE);
		return;
	}

	if (m_Saluting)
		Update_Salute(timeDelta);
	else
		Update_Wander(timeDelta);
}

void StateEm0010_Idle::Late_Update(Float timeDelta)
{
	
}

void StateEm0010_Idle::StateExitInvoke()
{
	
}

void StateEm0010_Idle::Update_Wander(Float timeDelta)
{
	m_WanderTimer -= timeDelta;

	if (m_WanderTimer <= 0.f)
	{
		m_WanderTimer = static_cast<Float>(Helper::Random_Double(3.0, 6.0));

		if (false == Helper::Random_Bool(0.5f))
			return;

		// A park gun machine fires a salute every other time it would set off.
		if (m_Owner.lock()->Is_Passive() && Is_Ranged() && Helper::Random_Bool(0.5f))
		{
			m_Saluting = true;
			m_SaluteFired = false;
			m_SaluteTime = 0.f;
			Em0010Movement::EM0010_MOVEMENT_DATA holdData{};
			holdData.isMove = false;
			holdData.canRotation = false;
			m_Movement.lock()->Set_MovementData(holdData);
			m_Body.lock()->Set_Animation(ETOI(Em0010::EM0010_STATE::SHOOT_START), 0.2f, false);
			return;
		}
		
		auto movement = m_Movement.lock();
		if (!movement) return;
		
		Vector3 rootPos = movement->Get_RootPosition();

		constexpr uint32 maxTry = 50;
		for (uint32 i = 0; i < maxTry; ++i)
		{
			Float randX = static_cast<Float>(Helper::Random_Double(-n_WanderRadius, n_WanderRadius));
			Float randZ = static_cast<Float>(Helper::Random_Double(-n_WanderRadius, n_WanderRadius));

			Vector3 randTargetPos = rootPos + Vector3{ randX, 0.f, randZ };
			Float dist = Vector3::Distance(randTargetPos, rootPos);
			if (dist > n_WanderRadius || dist < 5.f)
				continue;

			movement->Set_TargetPosition(randTargetPos);
			m_States.lock()->Change_State(MonsterStateMachine::MONSTER_STATE::WALK);
			return;
		}
	}
}

// SHOOT_START raises the gun, the shell leaves as it ends, SHOOT_END lowers it. Is_AnimationFinished answers for the
// previous clip while a blend runs, so each clip is given a moment first.
void StateEm0010_Idle::Update_Salute(Float timeDelta)
{
	auto em0010Body = m_Body.lock();
	m_SaluteTime += timeDelta;
	if (m_SaluteTime < 0.3f || !em0010Body->Is_AnimationFinished())
		return;

	m_SaluteTime = 0.f;
	if (!m_SaluteFired)
	{
		em0010Body->Fire_Firework();
		m_SaluteFired = true;
		em0010Body->Set_Animation(ETOI(Em0010::EM0010_STATE::SHOOT_END), 0.2f, false);
		return;
	}
	m_Saluting = false;
	em0010Body->Set_Animation(ETOI(Em0010::EM0010_STATE::IDLE), 0.2f, true);
}

Shared<StateEm0010_Idle> StateEm0010_Idle::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Idle>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0010_Idle");
		return nullptr;
	}

	return instance;
}
