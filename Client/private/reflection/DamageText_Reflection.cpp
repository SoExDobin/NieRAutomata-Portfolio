#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "DamageText.h"

ENGINE_REFLECTION_REGISTRATION(DamageText)
{
	ReflectedTypeBuilder<DamageText>(batch, "DamageText", "TextUI");
}
