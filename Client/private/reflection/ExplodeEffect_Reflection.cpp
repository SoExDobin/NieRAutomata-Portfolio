#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "ExplodeEffect.h"

ENGINE_REFLECTION_REGISTRATION(ExplodeEffect)
{
	ReflectedTypeBuilder<ExplodeEffect>(batch, "ExplodeEffect", "GameObject");
}
