#pragma once
#include "State.h"

NS_BEGIN(Client)

class Em0010;
class Em0010Body;
class Em0010Movement;
class MonsterStateMachine;

class CLIENT_DLL StateEm0010 abstract : public State
{
public:
	explicit StateEm0010(const wstring & tag, const Shared<Em0010>&owner);
	virtual ~StateEm0010() override;

protected:
	virtual HRESULT Initialize();

public:
	Float Get_DistanceToTarget() const;
	Vector3 Get_DirectionToTarget() const;
	Bool Has_Target() const;
	Bool Is_TargetFront() const;
	Bool Is_Ranged() const;
	Vector3 Get_TargetAimPosition() const;
	// Strafe band kept around the target (em0010BaseInfo DEFDIST: melee 4 m, ranged 10 m).
	Float Get_KeepDistanceMin() const { return Is_Ranged() ? n_RangedKeepMin : n_MeleeKeepMin; }
	Float Get_KeepDistanceMax() const { return Is_Ranged() ? n_RangedKeepMax : n_MeleeKeepMax; }
	// After an attack or a hit: strafe when still inside the band, otherwise run back in.
	void Change_ToEngageState();

protected:
	Weak<Em0010> m_Owner{};
	Weak<Em0010Body> m_Body{};
	Weak<Em0010Movement> m_Movement;
	Weak<MonsterStateMachine> m_States{};

protected:
	const Float n_LeastDistance{ 3.f };
	const Float n_WanderRadius{ 10.f };

	const Float n_MeleeKeepMin{ 3.5f };
	const Float n_MeleeKeepMax{ 6.f };
	const Float n_RangedKeepMin{ 8.f };
	const Float n_RangedKeepMax{ 14.f };
	const Float n_RangedMeleeDistance{ 2.5f };	// em0010EmInfo04 melee MAX_250
	const Float n_RangedEngageDistance{ 12.f };	// ranged stops chasing here and starts strafing
};

NS_END
