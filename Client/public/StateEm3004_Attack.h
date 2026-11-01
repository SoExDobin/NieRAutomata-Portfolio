#pragma once
#include "StateEm3004.h"

NS_BEGIN(Client)

// em3004AtkInfo TYPE_MACHINEGUN: after WAIT (65 frames) it raises the gun (0005), fires in the loop (0006, three shots a
// lap from the body's notifies) until ShotsPerReload are out, lowers it (0007) and waits ReloadWait before the next
// burst. It never turns: a burst starts only with the target in front of it (the half its body faces) and ends early
// when the target leaves it.
class CLIENT_DLL StateEm3004_Attack final : public StateEm3004
{
public:
	explicit StateEm3004_Attack(const wstring& tag, const Shared<Em3004>& owner);
	~StateEm3004_Attack() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

private:
	enum class PHASE : uint8 { WAIT, START, LOOP, END };

	PHASE m_Phase{ PHASE::WAIT };
	Float m_Wait{};

	static constexpr Float AimWait = 65.f / 60.f;

public:
	static Shared<StateEm3004_Attack> Create(const wstring& tag, const Shared<Em3004>& owner);
};

NS_END
