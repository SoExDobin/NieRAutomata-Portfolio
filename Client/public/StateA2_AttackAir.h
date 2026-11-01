#pragma once
#include "State2B_AttackAir.h"

NS_BEGIN(Client)

// A2 (Level 4) in the air: the right click spins up with the great sword (pl0000_0220) instead of 2B's plunge, once per
// air attack. A left click from the end of the spin until landing starts 2B's air combo once; otherwise she falls.
class CLIENT_DLL StateA2_AttackAir final : public State2B_AttackAir
{
public:
	explicit StateA2_AttackAir(const wstring& tag, const Shared<Pl0000>& owner);
	~StateA2_AttackAir() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Begin_HeavyDown() override;
	void Begin_RiseCombo();
	void End_Rise();

private:
	Bool m_IsRising{ false };
	Bool m_HasRisen{ false };
	// The rise gives one air combo, taken by a left click from n_RiseComboProgress until landing; one clicked earlier in the
	// spin waits for it.
	Bool m_RiseCombo{ false };
	Bool m_RiseComboQueued{ false };
	// The original pl0000_0220 seq frees the next action from frame 85 of 102, after the second hit.
	const Float n_RiseComboProgress{ 85.f / 102.f };

public:
	static Shared<StateA2_AttackAir> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END
