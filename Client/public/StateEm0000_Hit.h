#pragma once
#include "StateEm0000.h"

NS_BEGIN(Client)

// Flinches on the ground, or takes 2B's launch: rises, hangs, falls, lands, gets up.
class CLIENT_DLL StateEm0000_Hit final : public StateEm0000
{
public:
	explicit StateEm0000_Hit(const wstring& tag, const Shared<Em0000>& owner);
	~StateEm0000_Hit() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Start_AirLaunch();
	void Start_SlamDown(const Shared<GameObject>& plunger);

private:
	Bool Is_BackAttack() const;
	void Start_AirRehit();
	void Play_AirAnimation(uint32 animIndex, Bool loop);

private:
	enum class HIT_PHASE { GROUND, RISE, REHIT, HOVER, FALL, LAND, RECOVER };
	HIT_PHASE m_Phase{ HIT_PHASE::GROUND };
	Float m_HoverRemaining{};
	// The plunge that drives it down: it falls no higher than the plunger's feet until it lands.
	Weak<GameObject> m_SlamBy{};
	static constexpr Float AirHoverDuration = 0.35f;
	// The plunge's own gravity (State2B_AttackAir::Begin_HeavyDown), so it lands with 2B.
	static constexpr Float SlamGravity = 80.f;
	// The top of Em0000Body's AABB, and how fast a slammed body above the plunger's sword is drawn under it.
	static constexpr Float SlamBodyHeight = 1.4f;
	static constexpr Float SlamCatchUpSpeed = 30.f;

public:
	static Shared<StateEm0000_Hit> Create(const wstring& tag, const Shared<Em0000>& owner);
};

NS_END
