#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Phase 2 rest once its puppets are all down: the spots turn blue and the boss stands still (1000) open to attack for a
// while, then screams the next ones down (StateEm3000_Puppet).
class CLIENT_DLL StateEm3000_Phase2Groggy final : public StateEm3000
{
public:
	explicit StateEm3000_Phase2Groggy(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Phase2Groggy() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_Elapsed{};

	static constexpr Float Duration = 7.f;

public:
	static Shared<StateEm3000_Phase2Groggy> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
