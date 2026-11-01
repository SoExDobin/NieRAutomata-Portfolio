#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Phase 2 scream (1550), the spots crimson again: at the howl the boss drops em3004 puppets around the player
// (Em3000::Summon_Puppets, four as in the original, never more than four up at once), then goes back to the chase.
class CLIENT_DLL StateEm3000_Puppet final : public StateEm3000
{
public:
	explicit StateEm3000_Puppet(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Puppet() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Bool m_Summoned{ false };
	Float m_Elapsed{};

	// em3000_runaway_howl notify of 1550, in seconds (60 frames a second). Timed from the state's start, not by the clip's
	// progress, which is 0~1 and lags behind while the blend into 1550 runs.
	static constexpr Float SummonTime = 56.f / 60.f;

public:
	static Shared<StateEm3000_Puppet> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
