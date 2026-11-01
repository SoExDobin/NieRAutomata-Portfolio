#pragma once
#include "State2B_AttackGround.h"

NS_BEGIN(Client)

// A2 (Level 4) fights as 2B except for the right click: the great sword's four-hit light combo (pl0000_0200~0203)
// replaces 2B's three heavy hits, and Space cuts any of them short with a jump. The charge, the cross combo and the left
// click stay 2B's.
class CLIENT_DLL StateA2_AttackGround final : public State2B_AttackGround
{
public:
	explicit StateA2_AttackGround(const wstring& tag, const Shared<Pl0000>& owner);
	~StateA2_AttackGround() override = default;

private:
	HRESULT Initialize() override;

public:
	void Update(Float timeDelta) override;

public:
	static Shared<StateA2_AttackGround> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END
