#include "pch.h"
#include "StateEm3004_Attack.h"

#include "Em3004Body.h"

StateEm3004_Attack::StateEm3004_Attack(const wstring& tag, const Shared<Em3004>& owner)
	: StateEm3004{ tag, owner } {}

Bool StateEm3004_Attack::StateEnterInvoke()
{
	m_Phase = PHASE::WAIT;
	m_Wait = AimWait;
	Play(ANIM::IDLE, true, 0.2f);
	return true;
}

void StateEm3004_Attack::Update(Float timeDelta)
{
	Hold_Still();
	const Shared<Em3004> owner = m_Owner.lock();
	const Shared<Em3004Body> body = m_Body.lock();

	// The phase, not the model's clip index, picks the step: the index lags behind while a blend runs.
	switch (m_Phase)
	{
	case PHASE::WAIT:
		m_Wait -= timeDelta;
		if (m_Wait <= 0.f && owner->Is_TargetFront())
		{
			m_Phase = PHASE::START;
			Play(ANIM::SHOOT_START, false, 0.2f);
		}
		break;

	case PHASE::START:
		if (Is_Finished(ANIM::SHOOT_START))
		{
			m_Phase = PHASE::LOOP;
			body->Reset_ShotCount();
			Play(ANIM::SHOOT_LOOP, true);
		}
		break;

	case PHASE::LOOP:
		// A target gone to its side or behind it cuts the burst short.
		if (body->Get_ShotCount() >= owner->Get_ShotsPerReload() || !owner->Is_TargetFront())
		{
			m_Phase = PHASE::END;
			Play(ANIM::SHOOT_END, false, 0.2f);
		}
		break;

	case PHASE::END:
		if (Is_Finished(ANIM::SHOOT_END))
		{
			m_Phase = PHASE::WAIT;
			m_Wait = owner->Get_ReloadWait();
			Play(ANIM::IDLE, true, 0.2f);
		}
		break;
	}
}

Shared<StateEm3004_Attack> StateEm3004_Attack::Create(const wstring& tag, const Shared<Em3004>& owner)
{
	auto instance = make_shared<StateEm3004_Attack>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3004_Attack");
		return nullptr;
	}

	return instance;
}
