#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em8000Body.h"

ENGINE_REFLECTION_REGISTRATION(Em8000Body)
{
	ReflectedTypeBuilder<Em8000Body>(batch, "Em8000Body", "PartObject");
}
