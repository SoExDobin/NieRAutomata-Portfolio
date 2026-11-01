#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Phase 2 jump stomp (1500). Stomps again while the player stays close and a puppet is still up; with them all down it
// rests (phase 2 groggy) once the stomp has landed and ended.
class CLIENT_DLL StateEm3000_Stomp final : public StateEm3000
{
public:
	explicit StateEm3000_Stomp(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Stomp() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

public:
	static Shared<StateEm3000_Stomp> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
