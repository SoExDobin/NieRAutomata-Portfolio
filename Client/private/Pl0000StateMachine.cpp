#include "pch.h"
#include "Pl0000StateMachine.h"

#include "Pl0000.h"
#include "Pl0000Input.h"
#include "State2B.h"


Pl0000StateMachine::Pl0000StateMachine() : StateMachine{} {}
Pl0000StateMachine::Pl0000StateMachine(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: StateMachine{device, context} {}
Pl0000StateMachine::Pl0000StateMachine(const Pl0000StateMachine& rhs)
	: StateMachine{ rhs } {}

Bool Pl0000StateMachine::Change_State(Pl0000::PL0000_STATE state)
{
	wstring tag = Helper::To_wString(magic_enum::enum_name(state));
	
	return Change_StateByTag(tag);
}


Shared<State2B> Pl0000StateMachine::Find_2BState(Pl0000::PL0000_STATE state)
{
	wstring tag = Helper::To_wString(magic_enum::enum_name(state));

	return static_pointer_cast<State2B>(Find_State(tag));
}

wstring Pl0000StateMachine::Get_StateTag(Pl0000::PL0000_STATE state)
{
	if (Shared<State2B> searchedState = Find_2BState(state))
	{
		return Helper::To_wString(magic_enum::enum_name(state));
	}

	return L"";
}

Pl0000::PL0000_STATE Pl0000StateMachine::Get_CurPl0000State()
{
	if (nullptr == m_CurrentState)
		return static_cast<Pl0000::PL0000_STATE>(0);

	return magic_enum::enum_cast<Pl0000::PL0000_STATE>(Helper::To_String(Get_CurrentState()->Get_StateTag())).value();
}
	

HRESULT Pl0000StateMachine::Initialize_Prototype()
{
	return StateMachine::Initialize_Prototype();
}

HRESULT Pl0000StateMachine::Initialize(void* arg)
{
	if (FAILED(StateMachine::Initialize(arg)))
		return E_FAIL;

	return S_OK;
}

void Pl0000StateMachine::On_Destroy()
{
	StateMachine::On_Destroy();
}

void Pl0000StateMachine::Update_State(Float timeDelta)
{
	StateMachine::Update_State(timeDelta);
}

Shared<Pl0000StateMachine> Pl0000StateMachine::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000StateMachine>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000StateMachine");
		return nullptr;
	}

	return prototype;
}

Shared<Component> Pl0000StateMachine::Clone(void* arg)
{
	auto instance = make_shared<Pl0000StateMachine>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000StateMachine");
		return nullptr;
	}

	return instance;
}


