#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Half HP: the boss staggers (0300-0302); at the end the dress parts blow off and it transforms into phase 2.
class CLIENT_DLL StateEm3000_Groggy final : public StateEm3000
{
public:
	explicit StateEm3000_Groggy(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Groggy() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_LoopTime{};

	static constexpr Float LoopDuration = 7.f;		// as long as the phase 2 rest (StateEm3000_Phase2Groggy); 3 s was too short to punish
	static constexpr Float BlastSize = 20.f;

public:
	static Shared<StateEm3000_Groggy> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
