#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

class CLIENT_DLL StateEm0010_Attack final : public StateEm0010
{
public:
	explicit StateEm0010_Attack(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Attack() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Em0010Attack(); 
	void Update_Shoot(Float timeDelta);

private:
	unordered_set<uint32> m_EnterAnim;
	unordered_set<uint32> m_AttackAnim;

	// Ranged burst (em0010AtkInfo04: WAIT_10 frames between shots, TOTAL_NUM_8).
	Bool m_IsShooting{ false };
	uint32 m_ShotCount{};
	Float m_ShotTimer{};
	const Float n_ShotInterval{ 10.f / 60.f };
	const uint32 n_ShotsPerBurst{ 8 };

public:
	static Shared<StateEm0010_Attack> Create(const wstring& tag, const Shared<Em0010>& owner);
};

NS_END