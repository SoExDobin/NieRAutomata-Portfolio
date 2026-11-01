#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3004Body.h"

ENGINE_REFLECTION_REGISTRATION(Em3004Body)
{
	ReflectedTypeBuilder<Em3004Body>(batch, "Em3004Body", "PartObject");
}
