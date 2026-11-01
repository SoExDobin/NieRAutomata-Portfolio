#include "pch.h"
#include "StateEm0010_Dead.h"

#include <Game.h>

#include "Em0010.h"
#include "Em0010Body.h"
#include "Em0010Movement.h"
#include "ExplodeEffect.h"
#include "Random_Helper.h"

using em0010Anim = Em0010::EM0010_STATE;

StateEm0010_Dead::StateEm0010_Dead(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010{ tag, owner } {}
StateEm0010_Dead::~StateEm0010_Dead()
{
}

HRESULT StateEm0010_Dead::Initialize()
{
	return StateEm0010::Initialize();
}

Bool StateEm0010_Dead::StateEnterInvoke()
{
	m_Phase = PHASE::FALL;
	m_Timer = 0.f;
	m_BlowAway = m_Owner.lock()->Get_LastDamageInfo().blowAway;
	m_Body.lock()->Begin_Crack();

	if (m_BlowAway)
	{
		Enter_BlowAway();
		return true;
	}

	// AnimationTable DIE: a0190 is the only death motion (0x0300 is the stun clip).
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = false;
	moveData.isAttack = true;		// the collapse keeps its root motion
	moveData.canRotation = false;
	moveData.useRootMotionDir = true;
	m_Movement.lock()->Set_MovementData(moveData);

	m_Body.lock()->Set_Animation(ETOI(em0010Anim::DEAD_1), 0.2f, false);

	return true;
}

void StateEm0010_Dead::Update(Float timeDelta)
{
	auto body = m_Body.lock();
	m_Timer += timeDelta;

	switch (m_Phase)
	{
	case PHASE::FALL:
		if (m_BlowAway)
		{
			// DM_BLOW: the take-off clip, the flying loop while airborne, the fall on landing.
			if (body->Get_CurrentAnimationIndex() == ETOI(em0010Anim::BACK_DROP_1) && body->Is_AnimationFinished())
				body->Set_Animation(ETOI(em0010Anim::BACK_DROP_2), 0.1f, true);
			if (m_Timer >= n_MaxAirTime)
				Burst();
			else if (m_Timer >= 0.1f && m_Movement.lock()->Is_Grounded())
			{
				body->Set_Animation(ETOI(em0010Anim::BACK_DROP_3), 0.1f, false);
				m_Phase = PHASE::LAND;
			}
		}
		else if (body->Get_CurrentAnimationIndex() == ETOI(em0010Anim::DEAD_1) && body->Is_AnimationFinished())
			Burst();	// bursts as soon as the collapse ends
		break;

	case PHASE::LAND:
		if (body->Get_CurrentAnimationIndex() == ETOI(em0010Anim::BACK_DROP_3) && body->Is_AnimationFinished())
			Burst();
		break;

	case PHASE::SHATTER:
		if (body->Get_ShatterTime() >= Em0010Body::ShatterDuration)
			GAME_INSTANCE->Destroy(m_Owner.lock()->Get_ObjectGuid());
		break;
	}
}

void StateEm0010_Dead::Late_Update(Float timeDelta)
{

}

void StateEm0010_Dead::StateExitInvoke()
{

}

void StateEm0010_Dead::Enter_BlowAway()
{
	// Thrown straight away from the attacker while facing it.
	const auto owner = m_Owner.lock();
	Vector3 away = -owner->Get_Transform()->Get_Look();
	if (const auto attacker = owner->Get_LastDamageInfo().attacker.lock())
		away = owner->Get_Transform()->Get_Position() - attacker->Get_Transform()->Get_Position();
	away.y = 0.f;
	if (away.LengthSquared() < 0.0001f)
		away = -owner->Get_Transform()->Get_Look();
	away.Normalize();

	// A ballistic launch: Movement integrates gravity and clears the velocity on landing. No root motion on top.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = false;
	moveData.isAttack = false;
	moveData.canRotation = true;
	moveData.lookDirection = -away;
	auto movement = m_Movement.lock();
	movement->Set_MovementData(moveData);
	// The lift fixes the air time, so the horizontal speed alone sets how far it flies.
	const Float distanceScale = static_cast<Float>(Helper::Random_Double(1.0, n_BlowMaxScale));
	movement->Add_Force(away * n_BlowSpeed * distanceScale + Vector3{ 0.f, n_BlowLift, 0.f });

	m_Body.lock()->Set_Animation(ETOI(em0010Anim::BACK_DROP_1), 0.1f, false);
}

void StateEm0010_Dead::Burst()
{
	m_Phase = PHASE::SHATTER;
	m_Timer = 0.f;

	// The frozen pieces no longer follow root motion.
	Em0010Movement::EM0010_MOVEMENT_DATA stopData{};
	stopData.isMove = false;
	stopData.isAttack = false;
	stopData.canRotation = false;
	m_Movement.lock()->Set_MovementData(stopData);

	ExplodeEffect::EXPLODE_EFFECT_DESC explodeDesc{};
	explodeDesc.position = m_Body.lock()->Begin_Shatter();
	explodeDesc.size = n_ExplodeSize;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", m_Owner.lock()->Get_LevelIndex(), &explodeDesc);
}

Shared<StateEm0010_Dead> StateEm0010_Dead::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Dead>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Clone : StateEm0010_Dead");
		return nullptr;
	}

	return instance;
}
