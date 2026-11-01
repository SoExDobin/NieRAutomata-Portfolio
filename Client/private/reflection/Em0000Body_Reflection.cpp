#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em0000Body.h"

ENGINE_REFLECTION_REGISTRATION(Em0000Body)
{
	ReflectedTypeBuilder<Em0000Body>(batch, "Em0000Body", "PartObject");
}
