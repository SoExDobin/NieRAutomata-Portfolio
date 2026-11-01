#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

class CLIENT_DLL StateEm0010_Hit final : public StateEm0010
{
public:
	explicit StateEm0010_Hit(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Hit() override;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;
	void Start_AirLaunch();
	void Start_SlamDown(const Shared<GameObject>& plunger);

private:
	Bool Is_BackAttack();
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
	// The top of Em0010Body's AABB, and how fast a slammed body above the plunger's sword is drawn under it.
	static constexpr Float SlamBodyHeight = 2.f;
	static constexpr Float SlamCatchUpSpeed = 30.f;
	const Float n_ReChaseDistance = 8.f;

public:
	static Shared<StateEm0010_Hit> Create(const wstring& tag, const Shared<Em0010>& owner);
};

NS_END
