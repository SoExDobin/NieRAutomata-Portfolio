#include "pch.h"
#include "StateMachine.h"

#include <SpdLogger.h>

StateMachine::StateMachine() : ScriptComponent{} {}
StateMachine::StateMachine(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ScriptComponent{device, context} {}
StateMachine::StateMachine(const StateMachine& rhs)
	: ScriptComponent {rhs} {}

Shared<State> StateMachine::Get_CurrentState()
{
	return m_CurrentState;
}

HRESULT StateMachine::Add_State(const Shared<State>& state)
{
	if (!state)
	{
		LOG_ERROR(L"Cannot add a null state");
		return E_FAIL;
	}
	if (m_States.contains(state->Get_StateTag()))
	{
		LOG_ERROR(L"Failed to Add State {}", state->Get_StateTag());
		return E_FAIL;
	}

	m_States.emplace(state->Get_StateTag(), state);
	return S_OK;
}

Bool StateMachine::Change_StateByTag(const wstring& stateTag)
{
	if (!m_States.contains(stateTag)) 
	{
		LOG_ERROR(L"Failed to Change State");
		return false;
	}

	// TODO : 아직 State 변환 못함 제어 가 필요할까

	if (m_States[stateTag]->StateEnterInvoke())
	{
		if (m_CurrentState)
			m_CurrentState->StateExitInvoke();
		m_CurrentState = m_States[stateTag];
		return true;
	}
	return false;
}

Shared<State> StateMachine::Find_State(const wstring& stateTag)
{
	if (m_States.contains(stateTag))
	{
		return m_States[stateTag];
	}

	LOG_ERROR(L"Failed to Find State {}", stateTag);
	return nullptr;
}

HRESULT StateMachine::Initialize_Prototype()
{
	return ScriptComponent::Initialize_Prototype();
}

HRESULT StateMachine::Initialize(void* arg)
{
	return ScriptComponent::Initialize(arg);
}

void StateMachine::On_Destroy()
{
	m_States.clear();
	ScriptComponent::On_Destroy();
}

void StateMachine::Update_State(Float timeDelta)
{
	m_CurrentState->Update(timeDelta);

	m_CurrentState->Late_Update(timeDelta);
}
