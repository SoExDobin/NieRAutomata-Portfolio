#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "MonsterStateMachine.h"

ENGINE_REFLECTION_REGISTRATION(MonsterStateMachine)
{
	// Runtime values, read-only and unsaved.
	ReflectedTypeBuilder<MonsterStateMachine>(batch, "MonsterStateMachine", "ScriptComponent")
		.ReadOnly("CurrentState", &StateMachine::Get_CurrentStateTag);
}
