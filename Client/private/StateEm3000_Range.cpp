#include "pch.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "StateEm3000_Range.h"

#include "Em3000Body.h"
#include "EmLaser.h"
#include "Random_Helper.h"

StateEm3000_Range::StateEm3000_Range(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

HRESULT StateEm3000_Range::Initialize()
{
	if (FAILED(StateEm3000::Initialize())) return E_FAIL;
	m_Laser = static_pointer_cast<EmLaser>(m_Owner.lock()->Find_Child(L"EmLaser"));
	return m_Laser.expired() ? E_FAIL : S_OK;
}

Bool StateEm3000_Range::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	constexpr uint32 count = ETOI(PATTERN::END);
	m_Pattern = static_cast<PATTERN>(m_Visits < count ? m_Visits : Helper::Random_Int(0, count - 1));
	++m_Visits;
	m_LoopTime = 0.f;
	Play(Get_Clips(m_Pattern).start, false);
	// She sings to herself through her first barrage (the original bossTalk1 lines).
	if (m_Pattern == PATTERN::WAVE_BARRAGE || m_Pattern == PATTERN::STRAIGHT_BARRAGE || m_Pattern == PATTERN::RISING_BARRAGE)
		if (const auto client = ClientApp::GetInstance())
			client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_BULLET);
	return true;
}

void StateEm3000_Range::Update(Float timeDelta)
{
	const CLIPS clips = Get_Clips(m_Pattern);
	if (Is_Playing(clips.start))
		Hold_FacingTarget();

	if (Is_Finished(clips.start))
	{
		// A one-clip pattern (RING) attacks from its clip's notifies and is over when the clip ends.
		if (clips.start == clips.loop)
		{
			Change_State(STATE::IDLE);
			return;
		}
		// The laser's and the rising barrage's loop clips repeat until the attack is over.
		Play(clips.loop, m_Pattern == PATTERN::LASER || m_Pattern == PATTERN::RISING_BARRAGE);
		Begin_Attack();
		return;
	}
	if (Is_Playing(clips.loop))
		m_LoopTime += timeDelta;
	if (Is_Playing(clips.loop) && Is_AttackOver())
	{
		End_Attack();
		if (clips.hasEnd)
			Play(clips.end, false);
		else
			Change_State(STATE::IDLE);
		return;
	}
	if (clips.hasEnd && Is_Finished(clips.end))
		Change_State(STATE::IDLE);
}

void StateEm3000_Range::StateExitInvoke()
{
	End_Attack();
}

StateEm3000_Range::CLIPS StateEm3000_Range::Get_Clips(PATTERN pattern) const
{
	switch (pattern)
	{
	case PATTERN::LASER:
		return { ANIM::DANMAK_START_1, ANIM::DANMAK_LOOP_1, ANIM::DANMAK_END_1 };
	case PATTERN::HOMING:
		return { ANIM::DANMAK_START_2, ANIM::DANMAK_LOOP_2, ANIM::DANMAK_END_2 };
	case PATTERN::RING:
		return { ANIM::RING_THROW, ANIM::RING_THROW, ANIM::RING_THROW, false };
	case PATTERN::WAVE_BARRAGE:
	case PATTERN::RISING_BARRAGE:
		return { ANIM::DANMAK_START_1, ANIM::DANMAK_LOOP_1, ANIM::DANMAK_END_1 };
	case PATTERN::STRAIGHT_BARRAGE:
		return { ANIM::DANMAK_START_2, ANIM::DANMAK_LOOP_2, ANIM::DANMAK_END_2 };
	default:
		return { ANIM::DANMAK_START_3, ANIM::DANMAK_LOOP_3, ANIM::DANMAK_END_3 };
	}
}

void StateEm3000_Range::Begin_Attack() const
{
	const Shared<Em3000Body> body = m_Body.lock();
	// Danmak 1 opens fire with a shout (danmak 2 already charges with vo_h_charge, danmak 3 shouts on its first volley).
	const CLIPS clips = Get_Clips(m_Pattern);
	if (clips.loop == ANIM::DANMAK_LOOP_1)
		body->Play_Voice(L"em3000_vo_m_atk");
	switch (m_Pattern)
	{
	case PATTERN::LASER:
		m_Laser.lock()->Fire();
		break;
	case PATTERN::HOMING:
		body->Begin_Homing();
		break;
	case PATTERN::WAVE_BARRAGE:
		body->Begin_Barrage(Bullet::FLIGHT::WAVE);
		break;
	case PATTERN::STRAIGHT_BARRAGE:
		body->Begin_Barrage(Bullet::FLIGHT::STRAIGHT);
		break;
	case PATTERN::RISING_BARRAGE:
		body->Begin_Barrage(Bullet::FLIGHT::RISE);
		break;
	default:	// RING and HOWITZER fire from their clips' notifies
		break;
	}
}

Bool StateEm3000_Range::Is_AttackOver() const
{
	if (m_Pattern == PATTERN::LASER)
		return !m_Laser.lock()->Is_Running();
	if (m_Pattern == PATTERN::RISING_BARRAGE)
		return m_LoopTime >= RisingTime;
	return m_Body.lock()->Is_AnimationFinished();
}

void StateEm3000_Range::End_Attack() const
{
	m_Body.lock()->End_Barrage();
	m_Laser.lock()->Stop();
}

Shared<StateEm3000_Range> StateEm3000_Range::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Range>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Range");
		return nullptr;
	}

	return instance;
}
