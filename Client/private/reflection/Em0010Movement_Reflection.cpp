#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em0010Movement.h"

ENGINE_REFLECTION_REGISTRATION(Em0010Movement)
{
	// Runtime values, read-only and unsaved.
	ReflectedTypeBuilder<Em0010Movement>(batch, "Em0010Movement", "ScriptComponent")
		.ReadOnly("Grounded", &Movement::Is_Grounded)
		.ReadOnly("Gravity", &Movement::Get_Gravity)
		.ReadOnly("VerticalVelocity", &Movement::Get_VerticalVelocity)
		.ReadOnly("RootPosition", &Em0010Movement::Get_RootPosition)
		.ReadOnly("TargetPosition", &Em0010Movement::Get_TargetPosition);
}
