#include "pch.h"
#include "StateEm3000_Melee.h"

#include "Em3000Body.h"
#include "EmMovement.h"

StateEm3000_Melee::StateEm3000_Melee(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Melee::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	// The stomp only reaches a close player in front; anyone else gets the blade mixer.
	const Bool stomp = m_Owner.lock()->Is_TargetFront() && Get_DistanceToTarget() <= MeleeRange;
	Play(stomp ? ANIM::FRONT_STOMP : ANIM::MIXER_START, false);
	Hold_FacingTarget();
	return true;
}

void StateEm3000_Melee::Update(Float timeDelta)
{
	const Shared<Em3000> owner = m_Owner.lock();
	const Shared<EmMovement> movement = m_Movement.lock();

	if (Is_Finished(ANIM::FRONT_STOMP))
	{
		Change_State(STATE::IDLE);
		return;
	}
	if (Is_Finished(ANIM::MIXER_START))
	{
		Play(ANIM::MIXING, true);
		m_Origin = owner->Get_Transform()->Get_Position();
		m_MixerTime = 0.f;
		m_Body.lock()->Set_BodyAttack(true);
		m_Body.lock()->Play_LoopSound(L"em3000_knife_atk_loop");
		movement->Set_MovementData({});
		return;
	}
	if (Is_Playing(ANIM::MIXING))
	{
		// Out along a sine-shaped radius and back to the start while circling.
		m_MixerTime += timeDelta;
		const Float progress = min(m_MixerTime / MixerDuration, 1.f);
		const Float radius = MixerRadius * sinf(progress * XM_PI);
		const Vector3 next = m_Origin + Vector3{ cosf(m_MixerTime * MixerTurnRate) * radius, 0.f,
			sinf(m_MixerTime * MixerTurnRate) * radius };
		Vector3 delta = next - owner->Get_Transform()->Get_Position();
		delta.y = 0.f;
		movement->Add_Correction(delta);
		if (m_MixerTime >= MixerDuration)
		{
			m_MixerTime = 0.f;
			m_Body.lock()->Set_BodyAttack(false);
			m_Body.lock()->Stop_LoopSound();
			Play(ANIM::MIXER_END, false);
		}
		return;
	}
	if (Is_Playing(ANIM::MIXER_END))
	{
		// Glide the rest of the way back to where the mixer started.
		m_MixerTime += timeDelta;
		Vector3 delta = m_Origin - owner->Get_Transform()->Get_Position();
		delta.y = 0.f;
		movement->Add_Correction(delta * min(timeDelta / max(MixerReturnTime - m_MixerTime, timeDelta), 1.f));
		if (m_Body.lock()->Is_AnimationFinished())
			Change_State(STATE::IDLE);
	}
}

void StateEm3000_Melee::StateExitInvoke()
{
	m_Body.lock()->Set_BodyAttack(false);
	m_Body.lock()->Stop_LoopSound();
}

Shared<StateEm3000_Melee> StateEm3000_Melee::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Melee>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Melee");
		return nullptr;
	}

	return instance;
}
