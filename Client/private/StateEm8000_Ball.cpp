#include "pch.h"
#include "StateEm8000_Ball.h"

#include "ClientApp.h"
#include "Em8000Body.h"
#include "EmMovement.h"
#include "UIManager.h"

StateEm8000_Ball::StateEm8000_Ball(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Ball::StateEnterInvoke()
{
	const Shared<Em8000> owner = m_Owner.lock();
	if (owner->Is_Dead()) return false;
	owner->Enter_BallForm();
	if (const auto client = ClientApp::GetInstance())
		client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM8000_RAGE);
	m_Body.lock()->End_Attacks();
	m_Next = 0;
	Start(ANIM::BALL_IN, false, 0.2f);
	return true;
}

void StateEm8000_Ball::Start(ANIM clip, Bool loop, Float blend)
{
	m_Elapsed = 0.f;
	m_Clip = clip;
	Play(clip, loop, blend);
}

void StateEm8000_Ball::Start_Next()
{
	const PATTERN pattern = Cycle[m_Next];
	m_Next = (m_Next + 1) % static_cast<uint32>(std::size(Cycle));
	m_Body.lock()->Telegraph();
	switch (pattern)
	{
	case PATTERN::UTURN:
		m_Returning = false;
		Start(ANIM::BALL_DASH_START, false, 0.2f);
		break;
	case PATTERN::PRESS:
		Start(ANIM::BALL_PRESS, false, 0.2f);
		break;
	case PATTERN::SPIN:
		Start(ANIM::BALL_SPIN_START, false, 0.2f);
		break;
	}
}

void StateEm8000_Ball::Update(Float timeDelta)
{
	const Shared<Em8000Body> body = m_Body.lock();
	m_Elapsed += timeDelta;

	if (m_Clip == ANIM::BALL_IN)
	{
		Hold_Still();
		if (Is_Finished(ANIM::BALL_IN))
		{
			body->Hide_Armour();
			Start(ANIM::BALL_IDLE, true);
			m_Wait = WaitTime;
		}
		return;
	}
	if (m_Clip == ANIM::BALL_IDLE)
	{
		if (!Has_Target())
		{
			Hold_Still();
			return;
		}
		// With the player right under the ball the way to them flips about from frame to frame.
		if (Get_DistanceToTarget() < NearTarget)
			Hold_Still();
		else
			Hold_FacingTarget();
		m_Wait -= timeDelta;
		if (m_Wait <= 0.f)
			Start_Next();
		return;
	}
	if (m_Clip == ANIM::BALL_PRESS)
	{
		Update_Press();
		return;
	}
	if (m_Clip == ANIM::BALL_SPIN_START || m_Clip == ANIM::BALL_SPIN_LOOP || m_Clip == ANIM::BALL_SPIN_END)
	{
		Hold_Still();
		if (Is_Finished(ANIM::BALL_SPIN_START))
		{
			Start(ANIM::BALL_SPIN_LOOP, true);
			m_SpinTime = 0.f;
			m_RingTimer = 0.f;
			m_Rings = 0;
			body->Play_LoopSound(L"em8000_ball_side_spin_loop");
		}
		else if (m_Clip == ANIM::BALL_SPIN_LOOP)
		{
			m_RingTimer -= timeDelta;
			if (m_RingTimer <= 0.f)
			{
				m_RingTimer += RingInterval;
				body->Fire_Cone(RingShots, (m_Rings + 1) % 4 == 0);
				++m_Rings;
			}
			// MOT_WAIT: the lap running when SpinTime is up is played out.
			m_SpinTime += timeDelta;
			if (m_SpinTime >= ceilf(SpinTime / SpinLap) * SpinLap)
			{
				body->Stop_LoopSound();
				Start(ANIM::BALL_SPIN_END);
			}
		}
		else if (Is_Finished(ANIM::BALL_SPIN_END))
		{
			// 054D ends turned half round in its root.
			Turn_ToRoot();
			Start(ANIM::BALL_IDLE, true);
			m_Wait = WaitTime;
		}
		return;
	}
	Update_Dash();
}

void StateEm8000_Ball::Update_Dash()
{
	const Shared<Em8000Body> body = m_Body.lock();
	if (m_Clip == ANIM::BALL_DASH_START)
	{
		Follow_RootMotion(m_Elapsed < DashSteerTime);
		if (Is_Finished(ANIM::BALL_DASH_START))
		{
			Start(ANIM::BALL_DASH_LOOP);
			body->Play_LoopSound(L"em8000_ball_rolling_loop");
		}
	}
	else if (m_Clip == ANIM::BALL_DASH_LOOP)
	{
		// The roll back curves round onto the player as it sets off; with the player under the ball it keeps on straight.
		Follow_RootMotion(m_Returning && m_Elapsed < ReturnSteerTime && Get_DistanceToTarget() >= NearTarget);
		if (Is_Finished(ANIM::BALL_DASH_LOOP))
		{
			body->Stop_LoopSound();
			Start(m_Returning ? ANIM::BALL_DASH_END : ANIM::BALL_DASH_TURN);
		}
	}
	else if (m_Clip == ANIM::BALL_DASH_TURN)
	{
		// The U-turn is the clip's own (its root turns 172 degrees about the ball); the boss takes it over at the end so
		// the roll back starts from the turned pose, then steers on to the player.
		Follow_RootMotion(false);
		if (Is_Finished(ANIM::BALL_DASH_TURN))
		{
			Turn_ToRoot();
			m_Returning = true;
			Start(ANIM::BALL_DASH_LOOP);
			body->Play_LoopSound(L"em8000_ball_rolling_loop");
		}
	}
	else if (m_Clip == ANIM::BALL_DASH_END)
	{
		Follow_RootMotion(false);
		if (Is_Finished(ANIM::BALL_DASH_END))
		{
			Start(ANIM::BALL_DIZZY);
			body->Play_LoopSound(L"em8000_dizzy_loop");
		}
	}
	else if (m_Clip == ANIM::BALL_DIZZY)
	{
		Hold_Still();
		if (Is_Finished(ANIM::BALL_DIZZY))
		{
			body->Stop_LoopSound();
			// 0312, the ball's own way out of the dizzy spell, is not played; its sound is.
			body->Play_Sound(L"em8000_dizzy_end_ball");
			Start(ANIM::BALL_IDLE, true, 0.2f);
			m_Wait = WaitTime;
		}
	}
	else
		Hold_Still();
}

void StateEm8000_Ball::Update_Press()
{
	const Float frame = m_Elapsed * 60.f;
	const Float distance = Get_DistanceToTarget();
	const Bool underBall = !Has_Target() || distance < NearTarget;
	EmMovement::EM_MOVEMENT_DATA data{};
	if (frame < PressJumpFrame)
	{
		// The wind-up hop, turning to the player.
		data.isAttack = true;
		data.canRotation = !underBall;
		data.direction = Get_DirectionToTarget();
	}
	else if (frame < PressLandFrame)
	{
		// In the air it heads for where the player is now: each frame covers its share of the way left, so it comes
		// down on them however far they were or wherever they ran.
		const Float left = (std::max)((PressLandFrame - frame) * PressRootSpeed, PressRootSpeed);
		// isMove keeps the travel going where a slope lets go of the ground for a moment.
		data.isMove = true;
		data.isAttack = true;
		data.useRootMotionDir = false;
		data.canRotation = !underBall;
		data.direction = Get_DirectionToTarget();
		data.rootMotionScale = underBall ? 0.f : (std::min)(distance / left, PressMaxScale);
	}
	else
		data.canRotation = false;
	m_Movement.lock()->Set_MovementData(data);

	// Higher than the clip by BallPressLift along the clip's own rise and fall (its ball climbs 10 m as 1 - u^3 about the
	// top at 60, u = |frame - 60| / 30), so it still leaves and meets the floor where the clip does.
	const Float u = fabsf(frame - PressTopFrame) / PressAirHalf;
	m_Body.lock()->Set_Lift(u < 1.f ? m_Owner.lock()->Get_BallPressLift() * (1.f - u * u * u) : 0.f);

	if (Is_Finished(ANIM::BALL_PRESS))
	{
		Start(ANIM::BALL_IDLE, true, 0.2f);
		m_Wait = WaitTime;
	}
}

void StateEm8000_Ball::StateExitInvoke()
{
	m_Body.lock()->End_Attacks();
	m_Body.lock()->Set_Lift(0.f);
}

Shared<StateEm8000_Ball> StateEm8000_Ball::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Ball>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Ball");
		return nullptr;
	}

	return instance;
}
