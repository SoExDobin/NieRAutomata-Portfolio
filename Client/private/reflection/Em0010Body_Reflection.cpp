#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em0010Body.h"

ENGINE_REFLECTION_REGISTRATION(Em0010Body)
{
	ReflectedTypeBuilder<Em0010Body>(batch, "Em0010Body", "PartObject");
}
