#pragma once
#include "StateEm3004.h"

NS_BEGIN(Client)

// Summoned under the ceiling (Em3000::Summon_Puppets): the puppet hangs there a moment, then falls with its rubble holding
// 0001's first frame until its feet reach the floor, and 0001 drops it the last 8 m (lands at frame 17) and it stands up
// into the attack.
class CLIENT_DLL StateEm3004_Fall final : public StateEm3004
{
public:
	explicit StateEm3004_Fall(const wstring& tag, const Shared<Em3004>& owner);
	~StateEm3004_Fall() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Land();

private:
	Bool m_Dropping{ false };
	Bool m_HasFloor{ false };	// found below the spawn on the first update (the state starts before it is placed)
	Float m_FloorY{};
	Float m_FallSpeed{};
	Float m_HangTime{};

	static constexpr Float HangDuration = 0.5f;		// s under the ceiling before it drops
	static constexpr Float DropGravity = 15.f;		// m/s^2, half the movement's: about 1.4 s down a 14 m drop

public:
	static Shared<StateEm3004_Fall> Create(const wstring& tag, const Shared<Em3004>& owner);
};

NS_END
