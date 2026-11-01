#include "pch.h"
#include "StateEm0010_Hit.h"

#include <SpdLogger.h>
#include <Model.h>

#include "Em0010.h"
#include "Em0010Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

StateEm0010_Hit::StateEm0010_Hit(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010{tag, owner} {}
StateEm0010_Hit::~StateEm0010_Hit() {}


HRESULT StateEm0010_Hit::Initialize()
{

	return StateEm0010::Initialize();
}


Bool StateEm0010_Hit::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	using em0010Anim = Em0010::EM0010_STATE;
	if (!m_Movement.lock()->Is_Grounded())
	{
		Start_AirRehit();
		return true;
	}
	m_Phase = HIT_PHASE::GROUND;
	m_SlamBy.reset();
	m_HoverRemaining = 0.f;
	m_Movement.lock()->Set_Gravity(30.f);

	uint32 randIndex{};
	if (Is_BackAttack())
	{
		randIndex = Helper::Random_Int(ETOI(em0010Anim::HIT_BACK_1), ETOI(em0010Anim::HIT_BACK_3));
	}
	else
	{
		randIndex = Helper::Random_Int(ETOI(em0010Anim::HIT_FRONT_1), ETOI(em0010Anim::HIT_FRONT_3));
	}

	m_Body.lock()->Set_Animation(randIndex, 0.2f, false);

	return true;
}

void StateEm0010_Hit::Update(Float timeDelta)
{
	auto em0010 = m_Body.lock();

	const Entity::DAMAGE_INFO& dmgInfo = m_Owner.lock()->Get_LastDamageInfo();
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isMove = false;
	moveData.isAttack = true;         // 루트모션 넉백 위치 적용
	moveData.canRotation = false;
	moveData.useRootMotionDir = true;
	moveData.rootMotionScale = dmgInfo.knockbackForce;
	m_Movement.lock()->Set_MovementData(moveData);

	switch (m_Phase)
	{
	case HIT_PHASE::GROUND:
		if (em0010->Is_AnimationFinished() && m_Movement.lock()->Is_Grounded())
			Change_ToEngageState();
		break;
	case HIT_PHASE::RISE:
		if (!m_Movement.lock()->Is_Ascending())
		{
			m_Movement.lock()->Reset_VerticalVelocity();
			Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_HOVER), true);
			m_Phase = HIT_PHASE::HOVER;
		}
		break;
	case HIT_PHASE::REHIT:
		if (em0010->Is_AnimationFinished())
		{
			Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_HOVER), true);
			m_Phase = HIT_PHASE::HOVER;
		}
		break;
	case HIT_PHASE::HOVER:
		m_HoverRemaining -= timeDelta;
		if (m_HoverRemaining <= 0.f)
		{
			m_Movement.lock()->Set_Gravity(30.f);
			m_Phase = HIT_PHASE::FALL;
		}
		break;
	case HIT_PHASE::FALL:
		if (m_Movement.lock()->Is_Grounded())
		{
			m_SlamBy.reset();
			Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_LAND), false);
			m_Phase = HIT_PHASE::LAND;
		}
		else if (const auto plunger = m_SlamBy.lock())
		{
			// Hit higher than the plunger, it would land after it with the same gravity: it hurries under the sword, its
			// head below the plunger's feet.
			const auto transform = m_Owner.lock()->Get_Transform();
			const Float headAbove = transform->Get_Position().y + SlamBodyHeight * transform->Get_Scale().y
				- plunger->Get_Transform()->Get_Position().y;
			if (headAbove > 0.f)
				m_Movement.lock()->Add_Correction(Vector3{ 0.f, -min(headAbove, SlamCatchUpSpeed * timeDelta), 0.f });
		}
		break;
	case HIT_PHASE::LAND:
		if (em0010->Is_AnimationFinished())
		{
			Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_RECOVER), false);
			m_Phase = HIT_PHASE::RECOVER;
		}
		break;
	case HIT_PHASE::RECOVER:
		if (em0010->Is_AnimationFinished())
			Change_ToEngageState();
		break;
	}
}

void StateEm0010_Hit::Late_Update(Float timeDelta)
{
	
}

void StateEm0010_Hit::StateExitInvoke()
{
		
}

void StateEm0010_Hit::Start_AirLaunch()
{
	m_Phase = HIT_PHASE::RISE;
	m_SlamBy.reset();
	m_HoverRemaining = AirHoverDuration;
	m_Movement.lock()->Set_Gravity(0.f);
	Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_LAUNCH), false);
}

void StateEm0010_Hit::Start_SlamDown(const Shared<GameObject>& plunger)
{
	// Cuts the rise or hover short and drops; FALL lands it as usual.
	m_SlamBy = plunger;
	m_HoverRemaining = 0.f;
	m_Movement.lock()->Reset_VerticalVelocity();
	m_Movement.lock()->Set_Gravity(SlamGravity);
	m_Phase = HIT_PHASE::FALL;
	Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_REHIT), false);
}

void StateEm0010_Hit::Start_AirRehit()
{
	// A blow on the way down, the plunge's landing shockwave among them, does not stop the slam to hang it again.
	if (m_Phase == HIT_PHASE::FALL && !m_SlamBy.expired())
	{
		Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_REHIT), false);
		return;
	}
	const Bool stillRising = m_Phase == HIT_PHASE::RISE && m_Movement.lock()->Is_Ascending();
	m_HoverRemaining = AirHoverDuration;
	m_Movement.lock()->Set_Gravity(0.f);
	if (!stillRising)
	{
		m_Movement.lock()->Reset_VerticalVelocity();
		m_Phase = HIT_PHASE::REHIT;
	}
	Play_AirAnimation(ETOI(Em0010::EM0010_STATE::AIR_REHIT), false);
}

void StateEm0010_Hit::Play_AirAnimation(uint32 animIndex, Bool loop)
{
	auto body = m_Body.lock();
	auto model = body->Get_ModelComponent();
	// Repeated aerial hits restart their reaction, even when the same clip is already active.
	if (model->Get_AnimationIndex() == animIndex && !model->Is_Blending())
	{
		model->Set_AnimLoop(loop);
		model->Set_AnimationProgress(0.f);
	}
	else if (model->Is_Blending() && model->Get_NextAnimationIndex() == animIndex)
	{
		body->Set_Animation(animIndex, 0.f, loop);
	}
	else
	{
		body->Set_Animation(animIndex, 0.1f, loop);
	}
}

Bool StateEm0010_Hit::Is_BackAttack()
{
	auto& dmgInfo = m_Owner.lock()->Get_LastDamageInfo();
	Vector3 monsterLook = m_Owner.lock()->Get_Transform()->Get_Look();
	Vector3 toAttackPos = dmgInfo.hitPosition - m_Owner.lock()->Get_Transform()->Get_Position();
	
	Float scalar = monsterLook.Dot(toAttackPos);
	return scalar < 0.f;
}

Shared<StateEm0010_Hit> StateEm0010_Hit::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Hit>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Clone : StateEm0010_Hit");
		return nullptr;
	}

	return instance;
}

