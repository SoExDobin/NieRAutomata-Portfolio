#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000Movement.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000Movement)
{
	// Runtime values, read-only and unsaved.
	ReflectedTypeBuilder<Pl0000Movement>(batch, "Pl0000Movement", "ScriptComponent")
		.ReadOnly("Grounded", &Movement::Is_Grounded)
		.ReadOnly("Gravity", &Movement::Get_Gravity)
		.ReadOnly("VerticalVelocity", &Movement::Get_VerticalVelocity);
}
