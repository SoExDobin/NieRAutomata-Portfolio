#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

/* 0190: it collapses while blasts go off at bones all over the body, ends in one big blast at its centre, and the wreck
   stays where it fell (the boss is not removed). The ball first unfolds back onto its legs (0541, blasting already) and
   then collapses the same way (on request, 2026-10-24). */
class CLIENT_DLL StateEm8000_Dead final : public StateEm8000
{
public:
	explicit StateEm8000_Dead(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Dead() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_Elapsed{};
	Float m_NextBlast{};
	Bool m_Finished{};
	Bool m_Unfolding{};		// the ball's 0541 before 0190

	static constexpr Float BlastInterval = 0.2f;
	static constexpr Float ChainTime = 3.5f;		// the chain runs on past 0190's 112 frames
	static constexpr Float FinalBlastSize = 16.f;

public:
	static Shared<StateEm8000_Dead> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
