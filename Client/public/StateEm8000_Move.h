#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

// A hover step (em800f 0010-001b, EmInfo 後退/左移動/右移動): back off when the player is close, close in when far,
// otherwise slide to one side, facing the player throughout. Start, a short loop (the clips carry the travel), end.
class CLIENT_DLL StateEm8000_Move final : public StateEm8000
{
public:
	explicit StateEm8000_Move(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Move() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	ANIM m_Start{ ANIM::MOVE_L_START };		// the loop and the end clip follow it
	Float m_LoopTime{};
	Float m_LoopDuration{};

	static constexpr Float BackOffRange = 8.f;
	static constexpr Float CloseInRange = 26.f;
	static constexpr Float CloseInStop = 14.f;		// a forward step ends early once the player is this near
	static constexpr Double MinLoop = 0.35;			// one lap of the loop clips (21 frames) is about 7 m
	static constexpr Double MaxLoop = 0.7;

public:
	static Shared<StateEm8000_Move> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
