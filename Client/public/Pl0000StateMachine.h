#pragma once
#include "StateMachine.h"
#include "Pl0000.h"

NS_BEGIN(Client)

class State2B;

class Pl0000Input;

class CLIENT_DLL Pl0000StateMachine final : public StateMachine
{
public:
	typedef struct tagPl0000StateMachine : public STATEMACHINE_DESC
	{
			
	} PL0000_STATEMACHINE_DESC;

public:
	explicit Pl0000StateMachine();
	explicit Pl0000StateMachine(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000StateMachine(const Pl0000StateMachine& rhs);
	~Pl0000StateMachine() override = default;

public:
	Bool Change_State(Pl0000::PL0000_STATE state);
	Shared<State2B> Find_2BState(Pl0000::PL0000_STATE state);
	wstring Get_StateTag(Pl0000::PL0000_STATE state);
	Pl0000::PL0000_STATE Get_CurPl0000State();

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	void On_Destroy() override;
	void On_Disable() override { StateMachine::On_Disable(); }
	void On_Enable() override { StateMachine::On_Enable(); }

public:
	void Update_State(Float timeDelta) override;

public:
	static Shared<Pl0000StateMachine> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
