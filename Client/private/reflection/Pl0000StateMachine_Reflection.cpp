#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000StateMachine.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000StateMachine)
{
	// Runtime values, read-only and unsaved.
	ReflectedTypeBuilder<Pl0000StateMachine>(batch, "Pl0000StateMachine", "ScriptComponent")
		.ReadOnly("CurrentState", &StateMachine::Get_CurrentStateTag);
}
