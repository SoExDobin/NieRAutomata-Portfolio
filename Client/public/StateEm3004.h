#pragma once
#include "State.h"
#include "Em3004.h"

NS_BEGIN(Client)

class Em3004Body;
class EmMovement;

class CLIENT_DLL StateEm3004 abstract : public State
{
public:
	using STATE = Em3004::EM3004_STATE;
	using ANIM = Em3004::EM3004_ANIM;

public:
	explicit StateEm3004(const wstring& tag, const Shared<Em3004>& owner);
	virtual ~StateEm3004() override;

protected:
	virtual HRESULT Initialize();

protected:
	void Change_State(STATE state) const;
	void Play(ANIM anim, Bool loop, Float blend = 0.1f) const;
	Bool Is_Finished(ANIM anim) const;
	// The puppet never walks or turns: it keeps the facing it was dropped with. Flying, the movement leaves its height to
	// the state (the drop from the ceiling) and does not collide.
	void Hold_Still(Bool flying = false) const;

protected:
	Weak<Em3004> m_Owner{};
	Weak<Em3004Body> m_Body{};
	Weak<EmMovement> m_Movement{};
};

NS_END
