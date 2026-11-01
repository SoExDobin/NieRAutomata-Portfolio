#include "pch.h"
#include "MonsterStateMachine.h"

#include <String_Helper.h>
#include "StateMonster.h"

MonsterStateMachine::MonsterStateMachine() : StateMachine{} {}
MonsterStateMachine::MonsterStateMachine(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: StateMachine{ device, context } {}
MonsterStateMachine::MonsterStateMachine(const MonsterStateMachine& rhs)
	: StateMachine{ rhs } {}

Bool MonsterStateMachine::Change_State(MONSTER_STATE state)
{
	wstring tag = Helper::To_wString(magic_enum::enum_name(state));

	return Change_StateByTag(tag);
}

Shared<StateMonster> MonsterStateMachine::Find_MonsterState(MONSTER_STATE state)
{
	wstring tag = Helper::To_wString(magic_enum::enum_name(state));

	return static_pointer_cast<StateMonster>(Find_State(tag));
}

wstring MonsterStateMachine::Get_StateTag(MONSTER_STATE state)
{
	if (Shared<StateMonster> searchedState = Find_MonsterState(state))
	{
		return Helper::To_wString(magic_enum::enum_name(state));
	}

	return L"";
}

MonsterStateMachine::MONSTER_STATE MonsterStateMachine::Get_CurMonsterState()
{
	if (nullptr == m_CurrentState)
		return static_cast<MONSTER_STATE>(0);

	return magic_enum::enum_cast<MONSTER_STATE>(Helper::To_String(Get_CurrentState()->Get_StateTag())).value();
}


HRESULT MonsterStateMachine::Initialize_Prototype()
{
	return StateMachine::Initialize_Prototype();
}

HRESULT MonsterStateMachine::Initialize(void* arg)
{
	if (FAILED(StateMachine::Initialize(arg)))
		return E_FAIL;

	return S_OK;
}

void MonsterStateMachine::On_Destroy()
{
	StateMachine::On_Destroy();
}

void MonsterStateMachine::Update_State(Float timeDelta)
{
	StateMachine::Update_State(timeDelta);
}

Shared<MonsterStateMachine> MonsterStateMachine::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<MonsterStateMachine>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : MonsterStateMachine");
		return nullptr;
	}

	return prototype;
}

Shared<Component> MonsterStateMachine::Clone(void* arg)
{
	auto instance = make_shared<MonsterStateMachine>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : MonsterStateMachine");
		return nullptr;
	}

	return instance;
}


