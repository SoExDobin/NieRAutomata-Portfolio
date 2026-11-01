#include "pch.h"
#include "StateEm8000_Laser.h"

#include "Em8000Body.h"
#include "EmMovement.h"

StateEm8000_Laser::StateEm8000_Laser(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Laser::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	m_Elapsed = 0.f;
	Play(ANIM::LASER_START, false);
	m_Body.lock()->Telegraph();
	return true;
}

void StateEm8000_Laser::Update(Float timeDelta)
{
	// The spin is in the clip's root rotation, which stays in the pose; the transform stays put under it.
	m_Elapsed += timeDelta;
	if (m_Elapsed < AimTime)
	{
		const Vector3 toTarget = Get_DirectionToTarget();
		const Float yaw = atan2f(toTarget.x, toTarget.z) - XMConvertToRadians(LeadAngle);
		EmMovement::EM_MOVEMENT_DATA data{};
		data.lookDirection = Vector3{ sinf(yaw), 0.f, cosf(yaw) };
		m_Movement.lock()->Set_MovementData(data);
	}
	else
		Hold_Still();

	const Shared<Em8000Body> body = m_Body.lock();
	if (Is_Finished(ANIM::LASER_START))
	{
		Play(ANIM::LASER_LOOP, false, 0.05f);
		body->Play_LoopSound(L"em8000_hover_loop");
		return;
	}
	if (Is_Finished(ANIM::LASER_LOOP))
	{
		body->Stop_LoopSound();
		body->End_Laser();
		Play(ANIM::LASER_END, false, 0.05f);
		return;
	}
	if (Is_Finished(ANIM::LASER_END))
		Change_State(STATE::IDLE);
}

void StateEm8000_Laser::StateExitInvoke()
{
	m_Body.lock()->End_Attacks();
}

Shared<StateEm8000_Laser> StateEm8000_Laser::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Laser>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Laser");
		return nullptr;
	}

	return instance;
}
