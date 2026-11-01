#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

// Phase 1 melee: front stomp (0520) on a close player in front, otherwise the blade mixer (0510-0512) that spins the
// boss around a 10 m circle and back. Idle also sends a far player here now and then for the mixer.
class CLIENT_DLL StateEm3000_Melee final : public StateEm3000
{
public:
	explicit StateEm3000_Melee(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Melee() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Vector3 m_Origin{};
	Float m_MixerTime{};

	static constexpr Float MixerDuration = 10.f;
	static constexpr Float MixerRadius = 10.f;
	static constexpr Float MixerTurnRate = 4.f;		// rad/s around the circle
	static constexpr Float MixerReturnTime = 1.f;

public:
	static Shared<StateEm3000_Melee> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
