#pragma once
#include "State2B.h"

NS_BEGIN(Client)

class Pl0000;
class Pl0000EvadeGhost;

class CLIENT_DLL State2B_Evade final : public State2B
{
private:
	enum class DASH_DIR { FRONT, BACK, RIGHT, LEFT, NONE };
	enum class EVADE_PHASE { ENTER, DASH, EVADE, END };

public:
	explicit State2B_Evade(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_Evade() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void PlayEvadeSFX(EVADE_PHASE evadePhase) const;

public:
	Bool TryEvade_FromDash();

private:
	unordered_set<uint32> m_EnterAnim;
	unordered_set<uint32> m_DashAnim;
	unordered_set<uint32> m_EvadeAnim;

	DASH_DIR m_DashDir{ DASH_DIR::NONE };
	Float m_OriginalFov{ 0.f };
	EVADE_PHASE m_EvadePhase{ EVADE_PHASE::ENTER };

	// The texture flashes first, then streaks and four textured body echoes spread along the player's axes.
	Weak<Pl0000EvadeGhost> m_EvadeStreaks;
	vector<Weak<Pl0000EvadeGhost>> m_EvadeGhosts;
	Float m_GhostDelay{ 0.f };
	Bool m_StreakSpawned{ false };
	Bool m_CrossSpawned{ false };
	Bool m_PerfectEvade{ false };
	const Vector4 n_GhostColor{ 1.f, 1.f, 1.f, 0.34f };
	// The perfect dodge's invincibility afterwards; bullets still hit through it (Pl0000::TakeDamage).
	static constexpr Float PerfectEvadeInvincibleTime = 1.5f;

	// From State2B_Jump the dash is a short level burst with gravity off, then the jump takes the fall back.
	Bool m_InAir{ false };
	Vector3 m_AirDashDirection{};
	Float m_AirDashRemaining{};
	Float m_SavedGravity{};
	static constexpr Float AirDashTime = 0.35f;
	static constexpr Float AirDashSpeed = 14.f;

public:
	static Shared<State2B_Evade> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END

