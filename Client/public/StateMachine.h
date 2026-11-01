#pragma once
#include "State.h"
#include "ScriptComponent.h"

NS_BEGIN(Client)

class CLIENT_DLL StateMachine : public ScriptComponent
{
public:
	typedef struct tagStateMachine : public COMPONENT_DESC
	{} STATEMACHINE_DESC;
public:
	explicit StateMachine();
	explicit StateMachine(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit StateMachine(const StateMachine& rhs);
	~StateMachine() override = default;

public:
	Shared<State> Get_CurrentState();
	wstring Get_CurrentStateTag() const { return m_CurrentState ? m_CurrentState->Get_StateTag() : wstring{}; }
	HRESULT Add_State(const Shared<State>& state);
	Bool Change_StateByTag(const wstring& stateTag);
	Shared<State> Find_State(const wstring& stateTag);

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	void On_Destroy() override;
	void On_Disable() override { ScriptComponent::On_Disable(); }
	void On_Enable() override { ScriptComponent::On_Enable(); }

public:
	virtual void Update_State(Float timeDelta);

public:
	Shared<State> m_CurrentState{nullptr};
	unordered_map<wstring, Shared<State>> m_States;

public:
	virtual Shared<Component> Clone(void* arg = nullptr) PURE;
};

NS_END
