#pragma once
#include "State.h"

NS_BEGIN(Client)

class Monster;

class MonsterStateMachine;

class CLIENT_DLL StateMonster abstract : public State
{
public:
	explicit StateMonster(const wstring& tag, const Shared<Monster>& owner);
	virtual ~StateMonster() override;

protected:
	virtual HRESULT Initialize();

public:
	Vector3 Calculate_Direction();

protected:
	Weak<Monster> m_Owner{};


	Weak<MonsterStateMachine> m_States{};
};

NS_END
