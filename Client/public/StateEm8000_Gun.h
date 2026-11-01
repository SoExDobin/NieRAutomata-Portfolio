#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

/* The centre gun's two patterns (em8000EmInfo), picked by the idle state as Em8000::ATTACK:
   SHOTGUN: 0720, then 0721 looped TIME_180, one spread of ShotgunPellets orbs each 18-frame lap (its flag at 4), turning
   to the player; then 0722.
   SPIN_FIRE (回転発射): 0703, then 0704 looped TIME_450 while the body spins round in the clip, a spread of SpinPellets
   out of the barrel every SpinInterval wherever it points; then 0705.
   0703 and 0705 end turned in their root rotation; the boss takes the body's facing there (Turn_ToBody). */
class CLIENT_DLL StateEm8000_Gun final : public StateEm8000
{
public:
	explicit StateEm8000_Gun(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Gun() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Bool m_Shotgun{ true };
	ANIM m_Start{}, m_Loop{}, m_End{};
	Float m_Time{};
	Float m_NextShot{};

	// The spreads were widened on request (2026-10-22): the shotgun from 5 over 30 degrees, the spin from a single orb.
	static constexpr uint32 ShotgunPellets = 9;
	static constexpr Float ShotgunSpread = 50.f;			// degrees across the spread
	static constexpr uint32 SpinPellets = 3;
	static constexpr Float SpinSpread = 30.f;
	static constexpr Float ShotgunFirst = 4.f / 60.f;		// 0721 seq: its flag at 4
	static constexpr Float ShotgunInterval = 18.f / 60.f;	// one a lap
	static constexpr Float ShotgunTime = 180.f / 60.f;		// TIME_180
	static constexpr Float SpinInterval = 0.12f;			// 0704's flag is up the whole lap
	static constexpr Float SpinTime = 450.f / 60.f;			// TIME_450

public:
	static Shared<StateEm8000_Gun> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
