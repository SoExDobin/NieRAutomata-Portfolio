#pragma once
#include "StateEm3004.h"

NS_BEGIN(Client)

// A heavy hit (sword, pod laser) jerks the puppet on its strings (000f) and starts its attack over.
class CLIENT_DLL StateEm3004_Hit final : public StateEm3004
{
public:
	explicit StateEm3004_Hit(const wstring& tag, const Shared<Em3004>& owner);
	~StateEm3004_Hit() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

public:
	static Shared<StateEm3004_Hit> Create(const wstring& tag, const Shared<Em3004>& owner);
};

NS_END
