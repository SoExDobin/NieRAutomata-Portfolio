#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Phase 1 pause between attacks: turns to the player, then melee when close, otherwise a ranged pattern or now and then
// the blade mixer.
class CLIENT_DLL StateEm3000_Idle final : public StateEm3000
{
public:
	explicit StateEm3000_Idle(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Idle() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_Delay{};
	uint32 m_Visits{};	// decisions made, for the first round

	static constexpr Float FarMixerChance = 0.2f;

public:
	static Shared<StateEm3000_Idle> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
