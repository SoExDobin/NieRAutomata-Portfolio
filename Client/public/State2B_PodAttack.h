#pragma once
#include "State2B.h"

NS_BEGIN(Client)

class PodLaser;

// One press of E on the ground (a cooldown skill owned by Pl0000): the pod splits open and fires one beam where the camera looks.
// Input cuts the recoil short once the beam has faded; otherwise Idle takes over with the return clip after it.
class CLIENT_DLL State2B_PodAttack final : public State2B
{
public:
	explicit State2B_PodAttack(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_PodAttack() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Update_Facing();
	Bool Has_ActionInput() const;

private:
	// pl0000_1200 sways from the recoil until frame 210 of 328, then holds a still pose.
	const Float n_FireEnd = { 210.f / 328.f };
	// Input may cut the recoil short this long after the beam fires: its hit is over (PodLaser HitWindow 0.12 s) and the
	// beam fades on its own where it was fired.
	const Float n_CancelDelay = { 0.2f };

	Weak<PodLaser> m_Laser{};
	Bool m_Fired{ false };
	Float m_SinceFire{};

public:
	static Shared<State2B_PodAttack> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END
