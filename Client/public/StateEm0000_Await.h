#pragma once
#include "StateEm0000.h"

NS_BEGIN(Client)

// A short breather after an attack or a hit, facing the player; then it strikes again or runs back in.
class CLIENT_DLL StateEm0000_Await final : public StateEm0000
{
public:
	explicit StateEm0000_Await(const wstring& tag, const Shared<Em0000>& owner);
	~StateEm0000_Await() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

private:
	Float m_Remaining{};

public:
	static Shared<StateEm0000_Await> Create(const wstring& tag, const Shared<Em0000>& owner);
};

NS_END
