#include "pch.h"
#include "StateEm0000_Dead.h"

#include <Game.h>

#include "Em0000.h"
#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "ExplodeEffect.h"
#include "Random_Helper.h"

using em0000Anim = Em0000::EM0000_STATE;

StateEm0000_Dead::StateEm0000_Dead(const wstring& tag, const Shared<Em0000>& owner)
	: StateEm0000(tag, owner) {}

Bool StateEm0000_Dead::StateEnterInvoke()
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

	// The collapse keeps its root motion.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isAttack = true;
	moveData.canRotation = false;
	m_Movement.lock()->Set_MovementData(moveData);
	m_Body.lock()->Set_Animation(ETOI(em0000Anim::DEAD), 0.2f, false);

	return true;
}

void StateEm0000_Dead::Update(Float timeDelta)
{
	auto body = m_Body.lock();
	m_Timer += timeDelta;

	switch (m_Phase)
	{
	case PHASE::FALL:
		if (m_BlowAway)
		{
			// DM_BLOW: the take-off clip, the flying loop while airborne, the fall on landing.
			if (body->Get_CurrentAnimationIndex() == ETOI(em0000Anim::BLOW_START) && body->Is_AnimationFinished())
				body->Set_Animation(ETOI(em0000Anim::BLOW_LOOP), 0.1f, true);
			if (m_Timer >= n_MaxAirTime)
				Burst();
			else if (m_Timer >= 0.1f && m_Movement.lock()->Is_Grounded())
			{
				body->Set_Animation(ETOI(em0000Anim::BLOW_LAND), 0.1f, false);
				m_Phase = PHASE::LAND;
			}
		}
		else if (body->Get_CurrentAnimationIndex() == ETOI(em0000Anim::DEAD) && body->Is_AnimationFinished())
			Burst();
		break;

	case PHASE::LAND:
		if (body->Get_CurrentAnimationIndex() == ETOI(em0000Anim::BLOW_LAND) && body->Is_AnimationFinished())
			Burst();
		break;

	case PHASE::SHATTER:
		if (body->Get_ShatterTime() >= Em0000Body::ShatterDuration)
			GAME_INSTANCE->Destroy(m_Owner.lock()->Get_ObjectGuid());
		break;
	}
}

void StateEm0000_Dead::Enter_BlowAway()
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
	moveData.canRotation = true;
	moveData.lookDirection = -away;
	auto movement = m_Movement.lock();
	movement->Set_MovementData(moveData);
	const Float distanceScale = static_cast<Float>(Helper::Random_Double(1.0, n_BlowMaxScale));
	movement->Add_Force(away * n_BlowSpeed * distanceScale + Vector3{ 0.f, n_BlowLift, 0.f });

	m_Body.lock()->Set_Animation(ETOI(em0000Anim::BLOW_START), 0.1f, false);
}

void StateEm0000_Dead::Burst()
{
	m_Phase = PHASE::SHATTER;
	m_Timer = 0.f;

	// The frozen pieces no longer follow root motion.
	Em0010Movement::EM0010_MOVEMENT_DATA stopData{};
	stopData.canRotation = false;
	m_Movement.lock()->Set_MovementData(stopData);

	ExplodeEffect::EXPLODE_EFFECT_DESC explodeDesc{};
	explodeDesc.position = m_Body.lock()->Begin_Shatter();
	explodeDesc.size = n_ExplodeSize;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", m_Owner.lock()->Get_LevelIndex(), &explodeDesc);
}

Shared<StateEm0000_Dead> StateEm0000_Dead::Create(const wstring& tag, const Shared<Em0000>& owner)
{
	auto instance = make_shared<StateEm0000_Dead>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0000_Dead");
		return nullptr;
	}

	return instance;
}
