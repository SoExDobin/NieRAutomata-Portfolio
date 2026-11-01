#include "pch.h"
#include "StateEm8000_TailStab.h"

#include "Em8000Body.h"

StateEm8000_TailStab::StateEm8000_TailStab(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_TailStab::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_Time = 0.f;
	Play(Get_AngleToTarget() >= BehindAngle ? ANIM::TAIL_STAB_BACK : ANIM::TAIL_STAB_START, false);
	m_Body.lock()->Telegraph();
	return true;
}

void StateEm8000_TailStab::Update(Float timeDelta)
{
	Hold_Still();
	if (Is_Playing(ANIM::TAIL_STAB_START) || Is_Playing(ANIM::TAIL_STAB_BACK))
	{
		if (!m_Body.lock()->Is_AnimationFinished()) return;
		Turn_ToBody();
		Play(ANIM::TAIL_STAB_LOOP, true, 0.f);
		return;
	}
	if (Is_Playing(ANIM::TAIL_STAB_LOOP))
	{
		Turn_TailToTarget(timeDelta);
		// MOT_WAIT: past StabTime the lap in hand still plays out, so the drill is back in when 0557 starts.
		m_Time += timeDelta;
		const Float laps = ceilf(StabTime / StabLap);
		if (m_Time >= laps * StabLap)
			Play(ANIM::TAIL_STAB_END, false, 0.f);
		return;
	}
	if (Is_Finished(ANIM::TAIL_STAB_END))
	{
		Turn_ToBody();
		Play(ANIM::IDLE, true, 0.f);
		Change_State(STATE::IDLE);
	}
}

void StateEm8000_TailStab::StateExitInvoke()
{
	m_Body.lock()->End_Attacks();
}

void StateEm8000_TailStab::Turn_TailToTarget(Float timeDelta) const
{
	if (!Has_Target()) return;
	const Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	Vector3 look = transform->Get_Look();
	look.y = 0.f;
	look.Normalize();
	// The tail end points behind the boss: its back turns to the player.
	const Vector3 away = -Get_DirectionToTarget();
	const Float current = atan2f(look.x, look.z);
	const Float apart = remainderf(atan2f(away.x, away.z) - current, XM_2PI);
	const Float step = XMConvertToRadians(m_Owner.lock()->Get_TailTurnSpeed()) * timeDelta;
	const Float yaw = current + std::clamp(apart, -step, step);
	transform->Set_Rotation(Quaternion::CreateFromYawPitchRoll(yaw, 0.f, 0.f));
	transform->Update_WorldMatrix();
}

Shared<StateEm8000_TailStab> StateEm8000_TailStab::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_TailStab>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_TailStab");
		return nullptr;
	}

	return instance;
}
