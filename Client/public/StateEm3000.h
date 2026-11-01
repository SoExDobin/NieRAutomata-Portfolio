#pragma once
#include "State.h"
#include "Em3000.h"

NS_BEGIN(Client)

class Em3000Body;
class EmMovement;

class CLIENT_DLL StateEm3000 abstract : public State
{
public:
	using STATE = Em3000::EM3000_STATE;
	using ANIM = Em3000::EM3000_ANIM;

public:
	explicit StateEm3000(const wstring& tag, const Shared<Em3000>& owner);
	virtual ~StateEm3000() override;

protected:
	virtual HRESULT Initialize();

protected:
	Bool Has_Target() const;
	Float Get_DistanceToTarget() const;
	Vector3 Get_DirectionToTarget() const;
	void Change_State(STATE state) const;
	void Play(ANIM anim, Bool loop, Float blend = 0.1f) const;
	Bool Is_Playing(ANIM anim) const;
	Bool Is_Finished(ANIM anim) const;
	// Movement for the current step: stand still facing the target, or follow root motion (optionally steering).
	void Hold_FacingTarget() const;
	void Follow_RootMotion(Bool steerToTarget) const;

protected:
	Weak<Em3000> m_Owner{};
	Weak<Em3000Body> m_Body{};
	Weak<EmMovement> m_Movement{};

	static constexpr Float MeleeRange = 6.f;		// legacy Em3000MeleeSight radius
	static constexpr Float StompRange = 4.125f;		// phase 2 stops chasing and stomps inside this
};

NS_END
