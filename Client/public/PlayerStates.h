#pragma once
#include "State.h"

class C2B;

class C2B_State_Base : public State
{
public:
	explicit C2B_State_Base(Shared<C2B> pOwner) : m_pOwner(pOwner) {}
	virtual ~C2B_State_Base() override = default;

protected:
	Shared<C2B> m_pOwner = nullptr;
};

class C2B_Idle : public C2B_State_Base
{
public:
	using C2B_State_Base::C2B_State_Base;

	virtual void Enter() override;
	virtual void Update(Float timeDelta) override;
	virtual void Late_Update(Float timeDelta) override;
	virtual void Exit() override;
};

class C2B_Move : public C2B_State_Base
{
public:
	using C2B_State_Base::C2B_State_Base;

	virtual void Enter() override;
	virtual void Update(Float timeDelta) override;
	virtual void Late_Update(Float timeDelta) override;
	virtual void Exit() override;
};
