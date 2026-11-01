#pragma once
#include "StateMachine.h"

NS_BEGIN(Client)

class StateMonster;

class CLIENT_DLL MonsterStateMachine : public StateMachine
{
public:
	typedef struct tagMonsterStateMachine : public STATEMACHINE_DESC
	{
	} MONSTER_STATEMACHINE_DESC;

public:
	enum class MONSTER_STATE
	{
		IDLE,
		WALK,

		AWAIT,
		ATTACK,
		CHASE,
		RETURN,
		
		Hit,
		GROGGY,
		DEAD,
	};

public:
	explicit MonsterStateMachine();
	explicit MonsterStateMachine(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit MonsterStateMachine(const MonsterStateMachine& rhs);
	~MonsterStateMachine() override = default;

public:
	Bool Change_State(MONSTER_STATE state);
	Shared<StateMonster> Find_MonsterState(MONSTER_STATE state);
	wstring Get_StateTag(MONSTER_STATE state);
	MONSTER_STATE Get_CurMonsterState();

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	void On_Destroy() override;
	void On_Disable() override { StateMachine::On_Disable(); }
	void On_Enable() override { StateMachine::On_Enable(); }

public:
	void Update_State(Float timeDelta) override;

public:
	static Shared<MonsterStateMachine> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	virtual Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
