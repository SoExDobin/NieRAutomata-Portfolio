#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

// The eye laser (em8000EmInfo 銃用：レーザー発射, 0700 -> 0701 for 600 frames -> 0702): the beam charges and lands on
// the floor as far out as the player stands, and 0701 spins the body one full turn in 10 s, sweeping it round. The boss
// first turns to LeadAngle short of the player, so the beam starts beside the player and comes round to it once, to
// be jumped. Em8000Body starts the beam from 0700's notify; this state ends it.
class CLIENT_DLL StateEm8000_Laser final : public StateEm8000
{
public:
	explicit StateEm8000_Laser(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Laser() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_Elapsed{};

	static constexpr Float AimTime = 1.1f;		// it turns until just before 0700's laser flag (80)
	// The spin turns the eye toward positive yaw at 36 degrees a second: 60 short of the player, the beam reaches it
	// 1.7 s after it fires.
	static constexpr Float LeadAngle = 60.f;

public:
	static Shared<StateEm8000_Laser> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
