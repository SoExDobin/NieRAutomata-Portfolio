#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

// Death as in the original: a normal kill plays DIE (a0190), a blow-away kill (shockwave, pod laser,
// heavy sword) throws the body back on a real arc (DM_BLOW a0130 -> a0131 loop) and lands on its back (a0132).
// Either way it then bursts: an Explode sequence plus the frozen body's parts scattering and vanishing
// (Em0010Body::Begin_Shatter), then it is destroyed.
// Glowing fissures (Em0010Body::Begin_Crack) spread over the body from the moment it dies until the burst.
class CLIENT_DLL StateEm0010_Dead final : public StateEm0010
{
public:
	explicit StateEm0010_Dead(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Dead() override;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Enter_BlowAway();
	void Burst();

private:
	enum class PHASE : uint8 { FALL, LAND, SHATTER };

	PHASE m_Phase{ PHASE::FALL };
	Bool m_BlowAway{ false };
	Float m_Timer{};

	const Float n_BlowSpeed{ 12.f };		// horizontal launch, m/s (about 6 m of flight)
	const Float n_BlowMaxScale{ 2.5f };		// each launch goes 1x..2.5x as far, at random
	const Float n_BlowLift{ 8.f };			// vertical launch, m/s (Movement gravity 30)
	const Float n_MaxAirTime{ 3.f };		// bursts anyway if it never lands
	const Float n_ExplodeSize{ 4.f };

public:
	static Shared<StateEm0010_Dead> Create(const wstring& tag, const Shared<Em0010>& owner);
};

NS_END
