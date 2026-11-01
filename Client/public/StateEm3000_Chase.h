#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Phase 2: gets up to run (101a) and runs at the player (1010) until it is close enough to jump-stomp. With its puppets
// all down it stops to rest (StateEm3000_Phase2Groggy). It cries out as it sets off and every so often as it runs
// (em3000_vo_chase: vo_em3000 0075/0087/0089/0093, short random barks no seq places).
class CLIENT_DLL StateEm3000_Chase final : public StateEm3000
{
public:
	explicit StateEm3000_Chase(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Chase() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_VoiceWait{};	// until the next bark
	Float m_StompWait{};	// until the next stomp roll

	static constexpr Double VoiceMinWait = 2.5;
	static constexpr Double VoiceMaxWait = 4.5;
	// Inside StompRange it stomps on half its rolls; a miss keeps it after the player until the next roll.
	static constexpr Double StompChance = 0.5;
	static constexpr Float StompRetryTime = 1.f;

public:
	static Shared<StateEm3000_Chase> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
