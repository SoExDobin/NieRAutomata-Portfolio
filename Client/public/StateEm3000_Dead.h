#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Death: the boss screams (1550, its notifies shout and howl em3000_runaway_howl), cries as the shout dies away, and the
// moment the howl ends one big blast at the chest, with her death cry, breaks the body and the head chains (em3001) into
// their bones' pieces, which land, slide to a stop and dissolve (BoneShatter); its movement capsule goes. The hit boxes are off from the start
// (Em3000::OnDeath). The boss object is never removed.
class CLIENT_DLL StateEm3000_Dead final : public StateEm3000
{
public:
	explicit StateEm3000_Dead(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Dead() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_Elapsed{};
	Bool m_Finished{ false };

	// The howl starts at 1550 frame 56 and em3000_runaway_howl lasts 3.1 s.
	static constexpr Float BlastTime = 56.f / 60.f + 3.1f;
	static constexpr Float CryTime = 2.8f;		// em3000_vo_shout (3.4 s) fades from about 2.8 s
	static constexpr Float FinalBlastSize = 12.f;

public:
	static Shared<StateEm3000_Dead> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
