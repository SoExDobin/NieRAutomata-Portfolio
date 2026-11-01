#pragma once
#include "StateEm0000.h"

NS_BEGIN(Client)

// Collapses (DIE a0190), or is thrown back by a killing blow (DM_BLOW), cracking all the while; then it bursts into parts
// (Em0000Body::Begin_Shatter) and is destroyed. Same course as Em0010's death.
class CLIENT_DLL StateEm0000_Dead final : public StateEm0000
{
public:
	explicit StateEm0000_Dead(const wstring& tag, const Shared<Em0000>& owner);
	~StateEm0000_Dead() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

private:
	void Enter_BlowAway();
	void Burst();

private:
	enum class PHASE : uint8 { FALL, LAND, SHATTER };
	PHASE m_Phase{ PHASE::FALL };
	Float m_Timer{};
	Bool m_BlowAway{ false };
	const Float n_BlowSpeed{ 12.f };		// horizontal launch, m/s
	const Float n_BlowMaxScale{ 2.5f };		// each launch goes 1x..2.5x as far, at random
	const Float n_BlowLift{ 8.f };			// vertical launch, m/s (Movement gravity 30)
	const Float n_MaxAirTime{ 3.f };		// bursts anyway if it never lands
	const Float n_ExplodeSize{ 3.f };		// a smaller machine than Em0010 (4)

public:
	static Shared<StateEm0000_Dead> Create(const wstring& tag, const Shared<Em0000>& owner);
};

NS_END
