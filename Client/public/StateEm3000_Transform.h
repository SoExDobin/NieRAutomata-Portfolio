#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Plays the 100a transformation into the spider form under crimson spots, then screams its first puppets down.
class CLIENT_DLL StateEm3000_Transform final : public StateEm3000
{
public:
	explicit StateEm3000_Transform(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Transform() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:

public:
	static Shared<StateEm3000_Transform> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
