#pragma once
#include "State.h"

NS_BEGIN(Client)

class Em0000;
class Em0000Body;
class Em0010Movement;
class MonsterStateMachine;

class CLIENT_DLL StateEm0000 abstract : public State
{
public:
	explicit StateEm0000(const wstring& tag, const Shared<Em0000>& owner);
	virtual ~StateEm0000() override;

protected:
	virtual HRESULT Initialize();

public:
	Float Get_DistanceToTarget() const;
	Vector3 Get_DirectionToTarget() const;
	Bool Has_Target() const;
	Bool Is_TargetFront() const;
	// After an attack or a hit: pause facing the target, or stand idle when it is gone.
	void Change_ToEngageState();
	// Standing still, turning toward lookDirection when it is set.
	void Hold_Position(const Vector3& lookDirection = Vector3::Zero);

protected:
	Weak<Em0000> m_Owner{};
	Weak<Em0000Body> m_Body{};
	Weak<Em0010Movement> m_Movement{};
	Weak<MonsterStateMachine> m_States{};
};

NS_END
