#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "EmMovement.h"

ENGINE_REFLECTION_REGISTRATION(EmMovement)
{
	// Runtime values, read-only and unsaved.
	ReflectedTypeBuilder<EmMovement>(batch, "EmMovement", "ScriptComponent")
		.ReadOnly("Grounded", &Movement::Is_Grounded)
		.ReadOnly("Gravity", &Movement::Get_Gravity)
		.ReadOnly("VerticalVelocity", &Movement::Get_VerticalVelocity)
		.ReadOnly("RootPosition", &EmMovement::Get_RootPosition)
		.ReadOnly("TargetPosition", &EmMovement::Get_TargetPosition);
}
