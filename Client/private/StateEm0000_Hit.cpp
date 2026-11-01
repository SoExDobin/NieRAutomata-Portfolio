#include "pch.h"
#include "StateEm0000_Hit.h"

#include <Model.h>

#include "Em0000.h"
#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"

using em0000Anim = Em0000::EM0000_STATE;

StateEm0000_Hit::StateEm0000_Hit(const wstring& tag, const Shared<Em0000>& owner)
	: StateEm0000(tag, owner) {}

Bool StateEm0000_Hit::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	if (!m_Movement.lock()->Is_Grounded())
	{
		Start_AirRehit();
		return true;
	}
	m_Phase = HIT_PHASE::GROUND;
	m_SlamBy.reset();
	m_HoverRemaining = 0.f;
	m_Movement.lock()->Set_Gravity(30.f);

	// Always the short DM_MIDDLE flinch, as Em0010's: DM_BIG slid it 3.3 m off every sword blow and broke combos.
	const em0000Anim reaction = Is_BackAttack() ? em0000Anim::HIT_BACK : em0000Anim::HIT_FRONT;
	// Every blow restarts the flinch, even when the same clip is already playing.
	auto model = m_Body.lock()->Get_ModelComponent();
	if (model->Get_AnimationIndex() == ETOI(reaction) && !model->Is_Blending())
		model->Set_AnimationProgress(0.f);
	else
		m_Body.lock()->Set_Animation(ETOI(reaction), 0.1f, false);

	return true;
}

void StateEm0000_Hit::Update(Float timeDelta)
{
	auto body = m_Body.lock();
	auto movement = m_Movement.lock();

	// The reaction clips push it back along their root motion, as far as the blow's knockback says.
	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.isAttack = true;
	moveData.canRotation = false;
	moveData.rootMotionScale = m_Owner.lock()->Get_LastDamageInfo().knockbackForce;
	movement->Set_MovementData(moveData);

	switch (m_Phase)
	{
	case HIT_PHASE::GROUND:
		if (body->Is_AnimationFinished() && movement->Is_Grounded())
			Change_ToEngageState();
		break;
	case HIT_PHASE::RISE:
		if (!movement->Is_Ascending())
		{
			movement->Reset_VerticalVelocity();
			Play_AirAnimation(ETOI(em0000Anim::AIR_HOVER), true);
			m_Phase = HIT_PHASE::HOVER;
		}
		break;
	case HIT_PHASE::REHIT:
		if (body->Is_AnimationFinished())
		{
			Play_AirAnimation(ETOI(em0000Anim::AIR_HOVER), true);
			m_Phase = HIT_PHASE::HOVER;
		}
		break;
	case HIT_PHASE::HOVER:
		m_HoverRemaining -= timeDelta;
		if (m_HoverRemaining <= 0.f)
		{
			movement->Set_Gravity(30.f);
			m_Phase = HIT_PHASE::FALL;
		}
		break;
	case HIT_PHASE::FALL:
		if (movement->Is_Grounded())
		{
			m_SlamBy.reset();
			Play_AirAnimation(ETOI(em0000Anim::AIR_LAND), false);
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
				movement->Add_Correction(Vector3{ 0.f, -min(headAbove, SlamCatchUpSpeed * timeDelta), 0.f });
		}
		break;
	case HIT_PHASE::LAND:
		if (body->Is_AnimationFinished())
		{
			Play_AirAnimation(ETOI(em0000Anim::RECOVER), false);
			m_Phase = HIT_PHASE::RECOVER;
		}
		break;
	case HIT_PHASE::RECOVER:
		if (body->Is_AnimationFinished())
			Change_ToEngageState();
		break;
	}
}

void StateEm0000_Hit::Start_AirLaunch()
{
	m_Phase = HIT_PHASE::RISE;
	m_SlamBy.reset();
	m_HoverRemaining = AirHoverDuration;
	m_Movement.lock()->Set_Gravity(0.f);
	Play_AirAnimation(ETOI(em0000Anim::AIR_LAUNCH), false);
}

void StateEm0000_Hit::Start_SlamDown(const Shared<GameObject>& plunger)
{
	// Cuts the rise or hover short and drops; FALL lands it as usual.
	m_SlamBy = plunger;
	m_HoverRemaining = 0.f;
	m_Movement.lock()->Reset_VerticalVelocity();
	m_Movement.lock()->Set_Gravity(SlamGravity);
	m_Phase = HIT_PHASE::FALL;
	Play_AirAnimation(ETOI(em0000Anim::AIR_REHIT), false);
}

void StateEm0000_Hit::Start_AirRehit()
{
	// A blow on the way down, the plunge's landing shockwave among them, does not stop the slam to hang it again.
	if (m_Phase == HIT_PHASE::FALL && !m_SlamBy.expired())
	{
		Play_AirAnimation(ETOI(em0000Anim::AIR_REHIT), false);
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
	Play_AirAnimation(ETOI(em0000Anim::AIR_REHIT), false);
}

void StateEm0000_Hit::Play_AirAnimation(uint32 animIndex, Bool loop)
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
		body->Set_Animation(animIndex, 0.f, loop);
	else
		body->Set_Animation(animIndex, 0.1f, loop);
}

Bool StateEm0000_Hit::Is_BackAttack() const
{
	const auto owner = m_Owner.lock();
	const Vector3 toAttack = owner->Get_LastDamageInfo().hitPosition - owner->Get_Transform()->Get_Position();
	return owner->Get_Transform()->Get_Look().Dot(toAttack) < 0.f;
}

Shared<StateEm0000_Hit> StateEm0000_Hit::Create(const wstring& tag, const Shared<Em0000>& owner)
{
	auto instance = make_shared<StateEm0000_Hit>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0000_Hit");
		return nullptr;
	}

	return instance;
}
