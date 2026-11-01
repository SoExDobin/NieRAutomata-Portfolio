#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000Input.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000Input)
{
	// Runtime values, read-only and unsaved.
	ReflectedTypeBuilder<Pl0000Input>(batch, "Pl0000Input", "ScriptComponent")
		.ReadOnly("ComboCount", &Pl0000Input::Get_MouseComboCount);
}
