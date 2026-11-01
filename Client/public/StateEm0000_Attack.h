#pragma once
#include "StateEm0000.h"

NS_BEGIN(Client)

/* One attack picked by distance on entry (Em0000::Choose_Attack), its clips chained: the body tackle dives, slides on its
   belly and gets up; Emb001's trumpet is raised, blown for n_TrumpetTime while it keeps turning toward the player (Emb001
   fires the orbs) and lowered. It turns toward the player during the wind-up and commits after it. */
class CLIENT_DLL StateEm0000_Attack final : public StateEm0000
{
public:
	explicit StateEm0000_Attack(const wstring& tag, const Shared<Em0000>& owner);
	~StateEm0000_Attack() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_LoopTime{};
	const Float n_AimUntil{ 0.35f };	// progress of the wind-up during which it still turns
	const Float n_TrumpetTime{ 3.f };	// EmInfo statue trumpet attack: M1_05b1 LOOP TIME_180

public:
	static Shared<StateEm0000_Attack> Create(const wstring& tag, const Shared<Em0000>& owner);
};

NS_END
