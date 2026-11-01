#pragma once
#include "StateEm0000.h"

NS_BEGIN(Client)

// Runs at the player: strikes once it is within melee reach, and now and then opens the far attack (Em0000's tackle,
// Emb001's trumpet) from further out (Em0000::Get_AttackReach).
class CLIENT_DLL StateEm0000_Chase final : public StateEm0000
{
public:
	explicit StateEm0000_Chase(const wstring& tag, const Shared<Em0000>& owner);
	~StateEm0000_Chase() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

private:
	Float m_FarAttackCheck{};
	const Float n_FarAttackCheckInterval{ 1.f };

public:
	static Shared<StateEm0000_Chase> Create(const wstring& tag, const Shared<Em0000>& owner);
};

NS_END
