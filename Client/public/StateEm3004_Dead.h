#pragma once
#include "StateEm3004.h"

NS_BEGIN(Client)

// Death: the puppet bursts in an explosion, its parts fly apart and dissolve (Em3004Body::Begin_Shatter), then it is removed.
class CLIENT_DLL StateEm3004_Dead final : public StateEm3004
{
public:
	explicit StateEm3004_Dead(const wstring& tag, const Shared<Em3004>& owner);
	~StateEm3004_Dead() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

private:
	static constexpr Float ExplodeSize = 3.f;

public:
	static Shared<StateEm3004_Dead> Create(const wstring& tag, const Shared<Em3004>& owner);
};

NS_END
